#include "wifi_manager.hpp"
#include "wifi_logger.hpp"
#include <windows.h>
#include <wlanapi.h>
#include <memory>
#include <stdexcept>
#include <objbase.h>

// Hosted Network API is part of wlanapi.h, we don't need a separate header
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ole32.lib")

namespace wificpp {

// Implementation class definition
class WifiManager::Impl {
public:
    Impl() {
        DWORD negotiatedVersion;
        DWORD result = WlanOpenHandle(2, nullptr, &negotiatedVersion, &clientHandle);
        
        if (result != ERROR_SUCCESS) {
            throw std::runtime_error("Failed to open WLAN handle");
        }

        Logger::getInstance().info("WifiManager initialized");
    }

    ~Impl() {
        if (clientHandle != nullptr) {
            WlanCloseHandle(clientHandle, nullptr);
        }
    }

    std::vector<NetworkInfo> scan() {
        std::vector<NetworkInfo> networks;
        PWLAN_INTERFACE_INFO_LIST interfaceList = nullptr;
        
        DWORD result = WlanEnumInterfaces(clientHandle, nullptr, &interfaceList);
        if (result != ERROR_SUCCESS) {
            Logger::getInstance().error("Failed to enumerate WLAN interfaces");
            return networks;
        }

        std::unique_ptr<WLAN_INTERFACE_INFO_LIST, decltype(&WlanFreeMemory)> 
            interfaceListPtr(interfaceList, WlanFreeMemory);

        for (DWORD i = 0; i < interfaceListPtr->dwNumberOfItems; i++) {
            PWLAN_AVAILABLE_NETWORK_LIST networkList = nullptr;
            result = WlanScan(clientHandle, &interfaceListPtr->InterfaceInfo[i].InterfaceGuid, 
                            nullptr, nullptr, nullptr);
            
            if (result != ERROR_SUCCESS) {
                Logger::getInstance().warning("Failed to initiate scan on interface ", i);
                continue;
            }

            // Wait for scan to complete (you might want to make this async in production)
            Sleep(4000);

            result = WlanGetAvailableNetworkList(clientHandle, 
                                               &interfaceListPtr->InterfaceInfo[i].InterfaceGuid,
                                               0, nullptr, &networkList);

            if (result != ERROR_SUCCESS) {
                Logger::getInstance().warning("Failed to get network list for interface ", i);
                continue;
            }

            std::unique_ptr<WLAN_AVAILABLE_NETWORK_LIST, decltype(&WlanFreeMemory)> 
                networkListPtr(networkList, WlanFreeMemory);

            for (DWORD j = 0; j < networkListPtr->dwNumberOfItems; j++) {
                NetworkInfo info;
                auto& network = networkListPtr->Network[j];

                info.ssid = std::string(
                    reinterpret_cast<char*>(network.dot11Ssid.ucSSID),
                    network.dot11Ssid.uSSIDLength
                );
                info.signalStrength = (int)network.wlanSignalQuality;
                
                // Convert security type
                switch (network.dot11DefaultAuthAlgorithm) {
                    case DOT11_AUTH_ALGO_80211_OPEN:
                        info.security = SecurityType::NONE;
                        break;
                    case DOT11_AUTH_ALGO_80211_SHARED_KEY:
                        info.security = SecurityType::WEP;
                        break;
                    case DOT11_AUTH_ALGO_WPA:
                    case DOT11_AUTH_ALGO_WPA_PSK:
                        info.security = SecurityType::WPA;
                        break;
                    case DOT11_AUTH_ALGO_RSNA:
                    case DOT11_AUTH_ALGO_RSNA_PSK:
                        info.security = SecurityType::WPA2;
                        break;
                    default:
                        info.security = SecurityType::UNKNOWN;
                }

                networks.push_back(info);
            }
        }

        Logger::getInstance().info("Scan completed. Found ", networks.size(), " networks");
        return networks;
    }

    bool connect(const std::string& ssid, const std::string& password) {
        PWLAN_INTERFACE_INFO_LIST interfaceList = nullptr;
        DWORD result = WlanEnumInterfaces(clientHandle, nullptr, &interfaceList);
        
        if (result != ERROR_SUCCESS) {
            Logger::getInstance().error("Failed to enumerate WLAN interfaces");
            return false;
        }

        std::unique_ptr<WLAN_INTERFACE_INFO_LIST, decltype(&WlanFreeMemory)> 
            interfaceListPtr(interfaceList, WlanFreeMemory);

        if (interfaceListPtr->dwNumberOfItems == 0) {
            Logger::getInstance().error("No WLAN interfaces found");
            return false;
        }

        // For simplicity, use the first interface
        const auto& interfaceGuid = interfaceListPtr->InterfaceInfo[0].InterfaceGuid;

        // Create connection parameters
        WLAN_CONNECTION_PARAMETERS params = {};
        DOT11_SSID dot11Ssid = {};
        
        // Set SSID
        dot11Ssid.uSSIDLength = static_cast<ULONG>(ssid.size());
        memcpy_s(dot11Ssid.ucSSID, sizeof(dot11Ssid.ucSSID), ssid.c_str(), ssid.size());

        params.wlanConnectionMode = wlan_connection_mode_profile;
        params.strProfile = nullptr;  // We could create a profile here for more sophisticated connection management
        params.pDot11Ssid = &dot11Ssid;
        params.dwFlags = 0;

        result = WlanConnect(clientHandle, &interfaceGuid, &params, nullptr);
        
        if (result != ERROR_SUCCESS) {
            Logger::getInstance().error("Failed to connect to network: ", ssid);
            return false;
        }

        Logger::getInstance().info("Successfully connected to network: ", ssid);
        return true;
    }

    bool disconnect() {
        PWLAN_INTERFACE_INFO_LIST interfaceList = nullptr;
        DWORD result = WlanEnumInterfaces(clientHandle, nullptr, &interfaceList);
        
        if (result != ERROR_SUCCESS) {
            Logger::getInstance().error("Failed to enumerate WLAN interfaces");
            return false;
        }

        std::unique_ptr<WLAN_INTERFACE_INFO_LIST, decltype(&WlanFreeMemory)> 
            interfaceListPtr(interfaceList, WlanFreeMemory);

        if (interfaceListPtr->dwNumberOfItems == 0) {
            return false;
        }

        result = WlanDisconnect(clientHandle, 
                               &interfaceListPtr->InterfaceInfo[0].InterfaceGuid, 
                               nullptr);
        
        if (result != ERROR_SUCCESS) {
            Logger::getInstance().error("Failed to disconnect from network");
            return false;
        }

        Logger::getInstance().info("Successfully disconnected from network");
        return true;
    }

    ConnectionStatus getStatus() const {
        PWLAN_INTERFACE_INFO_LIST interfaceList = nullptr;
        DWORD result = WlanEnumInterfaces(clientHandle, nullptr, &interfaceList);
          if (result != ERROR_SUCCESS) {
            return ConnectionStatus::CONNECTION_ERROR;
        }

        std::unique_ptr<WLAN_INTERFACE_INFO_LIST, decltype(&WlanFreeMemory)> 
            interfaceListPtr(interfaceList, WlanFreeMemory);        if (interfaceListPtr->dwNumberOfItems == 0) {
            return ConnectionStatus::CONNECTION_ERROR;
        }

        switch (interfaceListPtr->InterfaceInfo[0].isState) {
            case wlan_interface_state_connected:
                return ConnectionStatus::CONNECTED;
            case wlan_interface_state_disconnected:
                return ConnectionStatus::DISCONNECTED;
            case wlan_interface_state_associating:
            case wlan_interface_state_discovering:
            case wlan_interface_state_authenticating:
                return ConnectionStatus::CONNECTING;            default:
                return ConnectionStatus::CONNECTION_ERROR;
        }
    }    bool isHotspotSupported() const {
        // MinGW doesn't fully support the Hosted Network API
        // For a real implementation, we would need to use Windows API directly
        // This is a placeholder implementation
        Logger::getInstance().warning("Hotspot functionality is not fully implemented in this build");
        return false;
    }
    
    bool isHotspotActive() const {
        // MinGW doesn't fully support the Hosted Network API
        // For a real implementation, we would need to use Windows API directly
        return false;
    }
    
    bool createHotspot(const std::string& ssid) {
        // MinGW doesn't fully support the Hosted Network API
        // For a real implementation, we would need to use Windows API directly
        Logger::getInstance().error("Hotspot creation is not supported in this build");
        return false;
    }
    
    bool stopHotspot() {
        // MinGW doesn't fully support the Hosted Network API
        // For a real implementation, we would need to use Windows API directly
        return false;
    }

private:
    HANDLE clientHandle = nullptr;
};

// Public interface implementation
WifiManager::WifiManager() : pimpl(std::make_unique<Impl>()) {}
WifiManager::~WifiManager() = default;

std::vector<NetworkInfo> WifiManager::scan() {
    return pimpl->scan();
}

bool WifiManager::connect(const std::string& ssid, const std::string& password) {
    return pimpl->connect(ssid, password);
}

bool WifiManager::disconnect() {
    return pimpl->disconnect();
}

ConnectionStatus WifiManager::getStatus() const {
    return pimpl->getStatus();
}

bool WifiManager::createHotspot(const std::string& ssid) {
    return pimpl->createHotspot(ssid);
}

bool WifiManager::stopHotspot() {
    return pimpl->stopHotspot();
}

bool WifiManager::isHotspotActive() const {
    return pimpl->isHotspotActive();
}

bool WifiManager::isHotspotSupported() const {
    return pimpl->isHotspotSupported();
}

} // namespace wificpp
