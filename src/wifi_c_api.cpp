#include "wifi_c_api.h"
#include "wifi_manager.hpp"
#include "wifi_logger.hpp"
#include <string>
#include <vector>
#include <cstring>

extern "C" {

// Create a new WifiManager instance
WifiManager* wifi_manager_new() {
    try {
        return reinterpret_cast<WifiManager*>(new wificpp::WifiManager());
    } catch (const std::exception& e) {
        wificpp::Logger::getInstance().error("Failed to create WifiManager: ", e.what());
        return nullptr;
    }
}

// Delete a WifiManager instance
void wifi_manager_delete(WifiManager* manager) {
    if (manager) {
        delete reinterpret_cast<wificpp::WifiManager*>(manager);
    }
}

// Helper function to convert wificpp::NetworkInfo to WifiNetworkInfo
static WifiNetworkInfo convert_network_info(const wificpp::NetworkInfo& info) {
    WifiNetworkInfo result;
    
    // Allocate memory for the C-style strings
    char* ssid = new char[info.ssid.length() + 1];
    std::strcpy(ssid, info.ssid.c_str());
    
    char* bssid = new char[info.bssid.length() + 1];
    std::strcpy(bssid, info.bssid.c_str());

    result.ssid = ssid;
    result.bssid = bssid;
    result.signal_strength = info.signalStrength;
    result.security_type = static_cast<int32_t>(info.security);
    result.channel = info.channel;
    result.frequency = info.frequency;
    
    return result;
}

// Scan for available networks
WifiNetworkInfo* wifi_manager_scan(WifiManager* manager, int* count) {
    if (!manager || !count) {
        return nullptr;
    }
    
    try {
        auto* wifiManager = reinterpret_cast<wificpp::WifiManager*>(manager);
        auto networks = wifiManager->scan();
        
        *count = static_cast<int>(networks.size());
        if (networks.empty()) {
            return nullptr;
        }
        
        // Allocate the array of network infos
        WifiNetworkInfo* result = new WifiNetworkInfo[*count];
        for (int i = 0; i < *count; i++) {
            result[i] = convert_network_info(networks[i]);
        }
        
        return result;
    } catch (const std::exception& e) {
        wificpp::Logger::getInstance().error("Failed to scan for networks: ", e.what());
        *count = 0;
        return nullptr;
    }
}

// Connect to a network
bool wifi_manager_connect(WifiManager* manager, const char* ssid, const char* password) {
    if (!manager || !ssid) {
        return false;
    }
    
    try {
        auto* wifiManager = reinterpret_cast<wificpp::WifiManager*>(manager);
        return wifiManager->connect(ssid, password ? password : "");
    } catch (const std::exception& e) {
        wificpp::Logger::getInstance().error("Failed to connect to network: ", e.what());
        return false;
    }
}

// Disconnect from the current network
bool wifi_manager_disconnect(WifiManager* manager) {
    if (!manager) {
        return false;
    }
    
    try {
        auto* wifiManager = reinterpret_cast<wificpp::WifiManager*>(manager);
        return wifiManager->disconnect();
    } catch (const std::exception& e) {
        wificpp::Logger::getInstance().error("Failed to disconnect from network: ", e.what());
        return false;
    }
}

// Get the current connection status
WifiConnectionStatus wifi_manager_get_status(WifiManager* manager) {
    if (!manager) {
        return WIFI_STATUS_ERROR;
    }
    
    try {
        auto* wifiManager = reinterpret_cast<wificpp::WifiManager*>(manager);
        auto status = wifiManager->getStatus();
        
        switch (status) {
            case wificpp::ConnectionStatus::CONNECTED:
                return WIFI_STATUS_CONNECTED;
            case wificpp::ConnectionStatus::DISCONNECTED:
                return WIFI_STATUS_DISCONNECTED;            case wificpp::ConnectionStatus::CONNECTING:
                return WIFI_STATUS_CONNECTING;
            case wificpp::ConnectionStatus::CONNECTION_ERROR:
            default:
                return WIFI_STATUS_ERROR;
        }
    } catch (const std::exception& e) {
        wificpp::Logger::getInstance().error("Failed to get connection status: ", e.what());
        return WIFI_STATUS_ERROR;
    }
}

// Free the network info array returned by wifi_manager_scan
void wifi_free_network_info(WifiNetworkInfo* networks, int count) {
    if (!networks || count <= 0) {
        return;
    }
    
    for (int i = 0; i < count; i++) {
        delete[] networks[i].ssid;
        delete[] networks[i].bssid;
    }
    
    delete[] networks;
}

// Create an unsecured WiFi hotspot with the given SSID
bool wifi_manager_create_hotspot(WifiManager* manager, const char* ssid) {
    if (!manager || !ssid) {
        return false;
    }
    
    try {
        auto* wifiManager = reinterpret_cast<wificpp::WifiManager*>(manager);
        return wifiManager->createHotspot(ssid);
    } catch (const std::exception& e) {
        wificpp::Logger::getInstance().error("Failed to create hotspot: ", e.what());
        return false;
    }
}

// Stop the active hotspot
bool wifi_manager_stop_hotspot(WifiManager* manager) {
    if (!manager) {
        return false;
    }
    
    try {
        auto* wifiManager = reinterpret_cast<wificpp::WifiManager*>(manager);
        return wifiManager->stopHotspot();
    } catch (const std::exception& e) {
        wificpp::Logger::getInstance().error("Failed to stop hotspot: ", e.what());
        return false;
    }
}

// Check if a hotspot is currently active
bool wifi_manager_is_hotspot_active(WifiManager* manager) {
    if (!manager) {
        return false;
    }
    
    try {
        auto* wifiManager = reinterpret_cast<wificpp::WifiManager*>(manager);
        return wifiManager->isHotspotActive();
    } catch (const std::exception& e) {
        wificpp::Logger::getInstance().error("Failed to check hotspot status: ", e.what());
        return false;
    }
}

// Check if the hardware supports hotspot functionality
bool wifi_manager_is_hotspot_supported(WifiManager* manager) {
    if (!manager) {
        return false;
    }
    
    try {
        auto* wifiManager = reinterpret_cast<wificpp::WifiManager*>(manager);
        return wifiManager->isHotspotSupported();
    } catch (const std::exception& e) {
        wificpp::Logger::getInstance().error("Failed to check hotspot support: ", e.what());
        return false;
    }
}

}
