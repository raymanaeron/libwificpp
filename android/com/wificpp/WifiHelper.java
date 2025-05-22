package com.wificpp;

import android.content.Context;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiInfo;
import android.net.NetworkInfo;
import android.net.ConnectivityManager;
import android.os.Build;
import android.net.wifi.WifiNetworkSpecifier;
import android.net.NetworkSpecifier;
import android.net.NetworkRequest;
import android.net.NetworkCallback;
import android.net.wifi.hotspot2.ConfigParser;
import android.net.wifi.hotspot2.PasspointConfiguration;
import android.net.wifi.SoftApConfiguration;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.IntentFilter;
import java.util.List;
import java.util.ArrayList;

public class WifiHelper {
    private static Context appContext;
    private WifiManager wifiManager;
    private ConnectivityManager connectivityManager;
    private NetworkCallback networkCallback;
    private List<ScanResult> latestScanResults = new ArrayList<>();
    private boolean scanning = false;
    
    // Initialize with application context
    public static void setAppContext(Context context) {
        appContext = context.getApplicationContext();
    }
    
    public WifiHelper() {
        // Empty constructor for JNI
    }
    
    public boolean initialize() {
        if (appContext == null) {
            return false;
        }
        
        wifiManager = (WifiManager) appContext.getSystemService(Context.WIFI_SERVICE);
        connectivityManager = (ConnectivityManager) appContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        
        if (wifiManager == null || connectivityManager == null) {
            return false;
        }
        
        // Register broadcast receiver for scan results
        BroadcastReceiver scanReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (WifiManager.SCAN_RESULTS_AVAILABLE_ACTION.equals(intent.getAction())) {
                    scanning = false;
                    latestScanResults = wifiManager.getScanResults();
                }
            }
        };
        
        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION);
        appContext.registerReceiver(scanReceiver, intentFilter);
        
        return true;
    }
    
    public boolean startScan() {
        if (wifiManager == null) {
            return false;
        }
        
        // Ensure WiFi is enabled
        if (!wifiManager.isWifiEnabled()) {
            wifiManager.setWifiEnabled(true);
        }
        
        scanning = true;
        boolean success = wifiManager.startScan();
        
        // If scan initiated successfully, wait for results
        if (success) {
            // Wait for scan to complete (max 10 seconds)
            int timeout = 0;
            while (scanning && timeout < 100) {
                try {
                    Thread.sleep(100);
                    timeout++;
                } catch (InterruptedException e) {
                    break;
                }
            }
        }
        
        return success;
    }
    
    public WifiNetwork[] getScanResults() {
        if (latestScanResults.isEmpty()) {
            return new WifiNetwork[0];
        }
        
        ArrayList<WifiNetwork> networks = new ArrayList<>();
        for (ScanResult result : latestScanResults) {
            WifiNetwork network = new WifiNetwork();
            network.ssid = result.SSID;
            network.bssid = result.BSSID;
            network.signalStrength = result.level;
            network.frequency = result.frequency;
            
            // Determine security type
            if (result.capabilities.contains("WEP")) {
                network.security = 1; // WEP
            } else if (result.capabilities.contains("WPA2")) {
                network.security = 3; // WPA2
            } else if (result.capabilities.contains("WPA")) {
                network.security = 2; // WPA
            } else {
                network.security = 0; // OPEN
            }
            
            networks.add(network);
        }
        
        return networks.toArray(new WifiNetwork[0]);
    }
    
    public boolean connect(String ssid, String password) {
        if (wifiManager == null) {
            return false;
        }
        
        // For Android 10+ (Q), use the new Network API
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            return connectAndroidQ(ssid, password);
        } else {
            return connectLegacy(ssid, password);
        }
    }
    
    private boolean connectLegacy(String ssid, String password) {
        WifiConfiguration config = new WifiConfiguration();
        config.SSID = "\"" + ssid + "\"";
        
        if (password.isEmpty()) {
            // Open network
            config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
        } else {
            // WPA/WPA2 network
            config.preSharedKey = "\"" + password + "\"";
        }
        
        int netId = wifiManager.addNetwork(config);
        if (netId == -1) {
            return false;
        }
        
        // Disconnect from current network
        wifiManager.disconnect();
        
        // Enable the new network
        boolean success = wifiManager.enableNetwork(netId, true);
        
        // Reconnect
        wifiManager.reconnect();
        
        return success;
    }
    
    private boolean connectAndroidQ(String ssid, String password) {
        if (networkCallback != null) {
            connectivityManager.unregisterNetworkCallback(networkCallback);
            networkCallback = null;
        }
        
        WifiNetworkSpecifier.Builder specifierBuilder = new WifiNetworkSpecifier.Builder();
        specifierBuilder.setSsid(ssid);
        
        if (!password.isEmpty()) {
            specifierBuilder.setWpa2Passphrase(password);
        }
        
        NetworkRequest request = new NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .setNetworkSpecifier(specifierBuilder.build())
            .build();
        
        final boolean[] connected = {false};
        networkCallback = new NetworkCallback() {
            @Override
            public void onAvailable(Network network) {
                // Use this network for all connections
                connectivityManager.bindProcessToNetwork(network);
                connected[0] = true;
            }
        };
        
        connectivityManager.requestNetwork(request, networkCallback);
        
        // Wait for connection to establish (max 10 seconds)
        int timeout = 0;
        while (!connected[0] && timeout < 100) {
            try {
                Thread.sleep(100);
                timeout++;
            } catch (InterruptedException e) {
                break;
            }
        }
        
        return connected[0];
    }
    
    public boolean disconnect() {
        if (wifiManager == null) {
            return false;
        }
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && networkCallback != null) {
            connectivityManager.unregisterNetworkCallback(networkCallback);
            networkCallback = null;
            connectivityManager.bindProcessToNetwork(null);
        }
        
        return wifiManager.disconnect();
    }
    
    public int getConnectionStatus() {
        if (wifiManager == null) {
            return 3; // CONNECTION_ERROR
        }
        
        if (!wifiManager.isWifiEnabled()) {
            return 0; // DISCONNECTED
        }
        
        NetworkInfo networkInfo = connectivityManager.getNetworkInfo(ConnectivityManager.TYPE_WIFI);
        if (networkInfo == null) {
            return 0; // DISCONNECTED
        }
        
        if (networkInfo.isConnected()) {
            return 2; // CONNECTED
        } else if (networkInfo.isConnectedOrConnecting()) {
            return 1; // CONNECTING
        } else {
            return 0; // DISCONNECTED
        }
    }
    
    public boolean createHotspot(String ssid, String password) {
        if (wifiManager == null) {
            return false;
        }
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            SoftApConfiguration.Builder configBuilder = new SoftApConfiguration.Builder();
            configBuilder.setSsid(ssid);
            
            if (!password.isEmpty()) {
                configBuilder.setPassphrase(password, SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
            }
            
            SoftApConfiguration config = configBuilder.build();
            return wifiManager.startTetheredHotspot(config);
        } else {
            WifiConfiguration config = new WifiConfiguration();
            config.SSID = ssid;
            
            if (!password.isEmpty()) {
                config.preSharedKey = password;
                config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_PSK);
            } else {
                config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
            }
            
            try {
                // Use reflection to call the method since it's not in the public API
                java.lang.reflect.Method method = wifiManager.getClass().getMethod(
                    "startSoftAp", WifiConfiguration.class);
                return (boolean) method.invoke(wifiManager, config);
            } catch (Exception e) {
                return false;
            }
        }
    }
    
    public boolean stopHotspot() {
        if (wifiManager == null) {
            return false;
        }
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            return wifiManager.stopSoftAp();
        } else {
            try {
                // Use reflection for older Android versions
                java.lang.reflect.Method method = wifiManager.getClass().getMethod("stopSoftAp");
                return (boolean) method.invoke(wifiManager);
            } catch (Exception e) {
                return false;
            }
        }
    }
    
    public boolean isHotspotActive() {
        if (wifiManager == null) {
            return false;
        }
        
        try {
            java.lang.reflect.Method method = wifiManager.getClass().getMethod("isWifiApEnabled");
            return (boolean) method.invoke(wifiManager);
        } catch (Exception e) {
            return false;
        }
    }
    
    public boolean isHotspotSupported() {
        if (wifiManager == null) {
            return false;
        }
        
        try {
            // Check if the methods exist
            wifiManager.getClass().getMethod("isWifiApEnabled");
            return true;
        } catch (NoSuchMethodException e) {
            return false;
        }
    }
}
