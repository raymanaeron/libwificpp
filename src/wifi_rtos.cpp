#include "wifi_impl.hpp"
#include "wifi_logger.hpp"
#include "wifi_platform.hpp"
#include "wifi_types.hpp"
#include <memory>
#include <stdexcept>
#include <cstring>

#ifdef WIFICPP_PLATFORM_RTOS

// Depending on the RTOS being used
#ifdef WIFICPP_FREERTOS
    #include "FreeRTOS.h"
    #include "task.h"
    #include "semphr.h"
#elif defined(WIFICPP_ZEPHYR)
    #include <zephyr.h>
    #include <net/net_if.h>
    #include <net/wifi_mgmt.h>
#elif defined(WIFICPP_THREADX)
    #include "tx_api.h"
#endif

// Hardware/platform-specific WiFi drivers
#if defined(WIFICPP_ESP32)
    #include "esp_wifi.h"
    #include "esp_event.h"
#elif defined(WIFICPP_STM32)
    #include "wifi.h"
    #include "wwd_wifi.h"
#endif

namespace wificpp {

class RTOSWifiImpl : public WifiImpl {
public:
    RTOSWifiImpl() {
        Logger::getInstance().info("WifiManager initialized on RTOS platform");
        
        // Initialize WiFi hardware
        bool initSuccess = initializeWiFiHardware();
        if (!initSuccess) {
            throw std::runtime_error("Failed to initialize WiFi hardware");
        }
    }
    
    ~RTOSWifiImpl() {
        // Deinitialize hardware if needed
        deinitializeWiFiHardware();
    }
    
    std::vector<NetworkInfo> scan() override {
        std::vector<NetworkInfo> networks;
        Logger::getInstance().info("Scanning for networks on RTOS");
        
#if defined(WIFICPP_ESP32)
        // ESP32 implementation
        wifi_scan_config_t scanConfig = {
            .ssid = NULL,
            .bssid = NULL,
            .channel = 0,
            .show_hidden = true,
            .scan_type = WIFI_SCAN_TYPE_ACTIVE,
            .scan_time = {
                .active = {
                    .min = 120,
                    .max = 150
                }
            }
        };
        
        // Start scan
        if (esp_wifi_scan_start(&scanConfig, true) != ESP_OK) {
            Logger::getInstance().error("Failed to start WiFi scan");
            return networks;
        }
        
        // Get scan results
        uint16_t apCount = 0;
        if (esp_wifi_scan_get_ap_num(&apCount) != ESP_OK) {
            Logger::getInstance().error("Failed to get AP count");
            return networks;
        }
        
        if (apCount == 0) {
            return networks;
        }
        
        // Allocate memory for results
        wifi_ap_record_t* list = new wifi_ap_record_t[apCount];
        if (esp_wifi_scan_get_ap_records(&apCount, list) != ESP_OK) {
            Logger::getInstance().error("Failed to get scan results");
            delete[] list;
            return networks;
        }
        
        // Convert to NetworkInfo objects
        for (uint16_t i = 0; i < apCount; i++) {
            NetworkInfo info;
            info.ssid = reinterpret_cast<char*>(list[i].ssid);
            
            char bssid[18] = {0};
            sprintf(bssid, "%02X:%02X:%02X:%02X:%02X:%02X",
                    list[i].bssid[0], list[i].bssid[1], list[i].bssid[2],
                    list[i].bssid[3], list[i].bssid[4], list[i].bssid[5]);
            info.bssid = bssid;
            
            info.channel = list[i].primary;
            info.frequency = 2407 + info.channel * 5;  // Approximate for 2.4GHz
            info.signalStrength = list[i].rssi;
            
            // Map authentication mode to security type
            switch (list[i].authmode) {
                case WIFI_AUTH_OPEN:
                    info.security = SecurityType::NONE;
                    break;
                case WIFI_AUTH_WEP:
                    info.security = SecurityType::WEP;
                    break;
                case WIFI_AUTH_WPA_PSK:
                    info.security = SecurityType::WPA;
                    break;
                case WIFI_AUTH_WPA2_PSK:
                case WIFI_AUTH_WPA_WPA2_PSK:
                    info.security = SecurityType::WPA2;
                    break;
                default:
                    info.security = SecurityType::UNKNOWN;
            }
            
            networks.push_back(info);
        }
        
        delete[] list;
        
#elif defined(WIFICPP_ZEPHYR)
        // Zephyr implementation
        struct net_if *iface = net_if_get_default();
        if (!iface) {
            Logger::getInstance().error("No network interface found");
            return networks;
        }
        
        // Start scan
        struct wifi_scan_params params = { 0 };
        params.scan_type = WIFI_SCAN_TYPE_ACTIVE;
        
        if (net_mgmt(NET_REQUEST_WIFI_SCAN, iface, &params, sizeof(params))) {
            Logger::getInstance().error("Failed to start WiFi scan");
            return networks;
        }
        
        // Wait for scan to complete
        k_sleep(K_MSEC(5000));
        
        // Get scan results
        struct wifi_scan_result scan_result = { 0 };
        int idx = 0;
        
        while (true) {
            if (net_mgmt(NET_REQUEST_WIFI_SCAN_RESULT, iface, &scan_result, sizeof(scan_result))) {
                break;  // No more results
            }
            
            NetworkInfo info;
            info.ssid = reinterpret_cast<char*>(scan_result.ssid);
            
            char bssid[18] = {0};
            sprintf(bssid, "%02X:%02X:%02X:%02X:%02X:%02X",
                    scan_result.mac[0], scan_result.mac[1], scan_result.mac[2],
                    scan_result.mac[3], scan_result.mac[4], scan_result.mac[5]);
            info.bssid = bssid;
            
            info.channel = scan_result.channel;
            info.signalStrength = scan_result.rssi;
            
            // Map security type
            if (scan_result.security & WIFI_SECURITY_TYPE_NONE) {
                info.security = SecurityType::NONE;
            } else if (scan_result.security & WIFI_SECURITY_TYPE_WEP) {
                info.security = SecurityType::WEP;
            } else if (scan_result.security & WIFI_SECURITY_TYPE_WPA_PSK) {
                info.security = SecurityType::WPA;
            } else if (scan_result.security & WIFI_SECURITY_TYPE_WPA2_PSK) {
                info.security = SecurityType::WPA2;
            } else {
                info.security = SecurityType::UNKNOWN;
            }
            
            networks.push_back(info);
            idx++;
        }
#else
        // Generic implementation for other RTOS platforms
        Logger::getInstance().warning("WiFi scanning not implemented for this RTOS platform");
#endif
        
        return networks;
    }
    
    bool connect(const std::string& ssid, const std::string& password) override {
        Logger::getInstance().info("Connecting to network: " + ssid);
        
#if defined(WIFICPP_ESP32)
        // ESP32 implementation
        wifi_config_t wifi_config = { 0 };
        strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), ssid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
        
        if (!password.empty()) {
            strncpy(reinterpret_cast<char*>(wifi_config.sta.password), password.c_str(), sizeof(wifi_config.sta.password) - 1);
        }
        
        // Set WiFi mode to station
        if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
            Logger::getInstance().error("Failed to set WiFi mode");
            return false;
        }
        
        // Set WiFi configuration
        if (esp_wifi_set_config(WIFI_IF_STA, &wifi_config) != ESP_OK) {
            Logger::getInstance().error("Failed to set WiFi configuration");
            return false;
        }
        
        // Connect to AP
        if (esp_wifi_connect() != ESP_OK) {
            Logger::getInstance().error("Failed to connect to WiFi");
            return false;
        }
        
        // Wait for connection
        int retries = 20;
        wifi_ap_record_t ap_info;
        
        while (retries--) {
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                return true;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        
        Logger::getInstance().error("Failed to connect to WiFi within timeout");
        return false;
        
#elif defined(WIFICPP_ZEPHYR)
        // Zephyr implementation
        struct net_if *iface = net_if_get_default();
        if (!iface) {
            Logger::getInstance().error("No network interface found");
            return false;
        }
        
        struct wifi_connect_req_params params = { 0 };
        params.ssid = reinterpret_cast<const uint8_t*>(ssid.c_str());
        params.ssid_length = ssid.length();
        
        if (!password.empty()) {
            params.psk = reinterpret_cast<const uint8_t*>(password.c_str());
            params.psk_length = password.length();
            params.security = WIFI_SECURITY_TYPE_PSK;
        } else {
            params.security = WIFI_SECURITY_TYPE_NONE;
        }
        
        params.channel = WIFI_CHANNEL_ANY;
        params.timeout = 10000;  // 10 seconds
        
        if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params))) {
            Logger::getInstance().error("Failed to connect to WiFi");
            return false;
        }
        
        return true;
#else
        // Generic implementation for other RTOS
        Logger::getInstance().warning("WiFi connection not implemented for this RTOS platform");
        return false;
#endif
    }
    
    bool disconnect() override {
        Logger::getInstance().info("Disconnecting from network");
        
#if defined(WIFICPP_ESP32)
        // ESP32 implementation
        esp_err_t err = esp_wifi_disconnect();
        return (err == ESP_OK);
        
#elif defined(WIFICPP_ZEPHYR)
        // Zephyr implementation
        struct net_if *iface = net_if_get_default();
        if (!iface) {
            Logger::getInstance().error("No network interface found");
            return false;
        }
        
        if (net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0)) {
            Logger::getInstance().error("Failed to disconnect from WiFi");
            return false;
        }
        
        return true;
#else
        // Generic implementation
        Logger::getInstance().warning("WiFi disconnection not implemented for this RTOS platform");
        return false;
#endif
    }
    
    ConnectionStatus getStatus() const override {
#if defined(WIFICPP_ESP32)
        // ESP32 implementation
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            return ConnectionStatus::CONNECTED;
        }
        
        // Check if we're in the middle of connecting
        wifi_ps_type_t ps_type;
        if (esp_wifi_get_ps(&ps_type) == ESP_OK) {
            return ConnectionStatus::CONNECTING;  // Best approximation
        }
        
        return ConnectionStatus::DISCONNECTED;
        
#elif defined(WIFICPP_ZEPHYR)
        // Zephyr implementation
        struct net_if *iface = net_if_get_default();
        if (!iface) {
            return ConnectionStatus::CONNECTION_ERROR;
        }
        
        struct wifi_status status = { 0 };
        if (net_mgmt(NET_REQUEST_WIFI_STATUS, iface, &status, sizeof(status))) {
            return ConnectionStatus::CONNECTION_ERROR;
        }
        
        if (status.state == WIFI_STATE_CONNECTED) {
            return ConnectionStatus::CONNECTED;
        } else if (status.state == WIFI_STATE_CONNECTING) {
            return ConnectionStatus::CONNECTING;
        } else {
            return ConnectionStatus::DISCONNECTED;
        }
#else
        // Generic implementation
        Logger::getInstance().warning("WiFi status check not implemented for this RTOS platform");
        return ConnectionStatus::CONNECTION_ERROR;
#endif
    }
    
    bool createHotspot(const std::string& ssid, const std::string& password = "") override {
        Logger::getInstance().info("Creating hotspot: " + ssid);
        
#if defined(WIFICPP_ESP32)
        // ESP32 implementation
        wifi_config_t ap_config = { 0 };
        strncpy(reinterpret_cast<char*>(ap_config.ap.ssid), ssid.c_str(), sizeof(ap_config.ap.ssid) - 1);
        ap_config.ap.ssid_len = ssid.length();
        
        if (!password.empty()) {
            strncpy(reinterpret_cast<char*>(ap_config.ap.password), password.c_str(), sizeof(ap_config.ap.password) - 1);
            ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
        } else {
            ap_config.ap.authmode = WIFI_AUTH_OPEN;
        }
        
        ap_config.ap.max_connection = 4;
        ap_config.ap.channel = 1;
        
        // Set WiFi mode to AP
        if (esp_wifi_set_mode(WIFI_MODE_AP) != ESP_OK) {
            Logger::getInstance().error("Failed to set WiFi mode to AP");
            return false;
        }
        
        // Configure AP
        if (esp_wifi_set_config(WIFI_IF_AP, &ap_config) != ESP_OK) {
            Logger::getInstance().error("Failed to configure WiFi AP");
            return false;
        }
        
        // Start AP
        if (esp_wifi_start() != ESP_OK) {
            Logger::getInstance().error("Failed to start WiFi AP");
            return false;
        }
        
        return true;
#else
        // Other platforms
        Logger::getInstance().warning("Hotspot functionality not implemented for this RTOS platform");
        return false;
#endif
    }
    
    bool stopHotspot() override {
        Logger::getInstance().info("Stopping hotspot");
        
#if defined(WIFICPP_ESP32)
        // ESP32 implementation
        if (esp_wifi_stop() != ESP_OK) {
            Logger::getInstance().error("Failed to stop WiFi AP");
            return false;
        }
        
        return true;
#else
        // Other platforms
        Logger::getInstance().warning("Hotspot stopping not implemented for this RTOS platform");
        return false;
#endif
    }
    
    bool isHotspotActive() const override {
#if defined(WIFICPP_ESP32)
        // ESP32 implementation
        wifi_mode_t mode;
        if (esp_wifi_get_mode(&mode) != ESP_OK) {
            return false;
        }
        
        return (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA);
#else
        // Other platforms
        return false;
#endif
    }
    
    bool isHotspotSupported() const override {
#if defined(WIFICPP_ESP32) || defined(WIFICPP_NRF52)
        return true;
#else
        return false;
#endif
    }
    
private:
    bool initializeWiFiHardware() {
#if defined(WIFICPP_ESP32)
        // Initialize the TCP/IP stack
        esp_err_t err = tcpip_adapter_init();
        if (err != ESP_OK) {
            Logger::getInstance().error("Failed to initialize TCP/IP adapter");
            return false;
        }
        
        // Initialize the WiFi event loop
        err = esp_event_loop_create_default();
        if (err != ESP_OK) {
            Logger::getInstance().error("Failed to create WiFi event loop");
            return false;
        }
        
        // Initialize the WiFi driver
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        err = esp_wifi_init(&cfg);
        if (err != ESP_OK) {
            Logger::getInstance().error("Failed to initialize WiFi");
            return false;
        }
        
        // Start the WiFi driver
        err = esp_wifi_start();
        if (err != ESP_OK) {
            Logger::getInstance().error("Failed to start WiFi");
            return false;
        }
        
        return true;
#elif defined(WIFICPP_ZEPHYR)
        // Zephyr WiFi is initialized by the system
        return true;
#else
        // Generic initialization
        Logger::getInstance().info("Using default WiFi hardware initialization");
        return true;
#endif
    }
    
    void deinitializeWiFiHardware() {
#if defined(WIFICPP_ESP32)
        esp_wifi_stop();
        esp_wifi_deinit();
#endif
        // Other platforms may not need explicit deinitialization
    }
};

// Factory function implementation for RTOS
std::unique_ptr<WifiImpl> createPlatformImpl() {
    return std::make_unique<RTOSWifiImpl>();
}

} // namespace wificpp

#endif // WIFICPP_PLATFORM_RTOS
