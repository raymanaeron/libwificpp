#include "wifi_impl.hpp"
#include "wifi_logger.hpp"
#include "wifi_platform.hpp"
#include "wifi_types.hpp"
#include <memory>
#include <stdexcept>

#ifdef WIFICPP_PLATFORM_WINDOWS

#include <windows.h>
#include <wlanapi.h>
#include <objbase.h>
#include <algorithm>
#include <codecvt>
#include <locale>

#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ole32.lib")

namespace {
// Helper function to convert UTF-8 string to UTF-16 (wide) string
std::wstring utf8ToWide(const std::string& utf8Str) {
    try {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.from_bytes(utf8Str);
    } catch (const std::exception&) {
        return std::wstring(utf8Str.begin(), utf8Str.end());
    }
}

// Helper function to convert UTF-16 (wide) string to UTF-8
std::string wideToUtf8(const wchar_t* wideStr, size_t len = static_cast<size_t>(-1)) {
    try {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.to_bytes(wideStr, wideStr + (len == static_cast<size_t>(-1) ? wcslen(wideStr) : len));
    } catch (const std::exception&) {
        return std::string(wideStr, wideStr + (len == static_cast<size_t>(-1) ? wcslen(wideStr) : len));
    }
}
} // anonymous namespace

namespace wificpp {

class WindowsWifiImpl : public WifiImpl {
public:
    WindowsWifiImpl() {
        DWORD negotiatedVersion;
        DWORD result = WlanOpenHandle(2, nullptr, &negotiatedVersion, &clientHandle);
        
        if (result != ERROR_SUCCESS) {
            throw std::runtime_error("Failed to open WLAN handle");
        }

        Logger::getInstance().info("WifiManager initialized on Windows platform");
    }

    ~WindowsWifiImpl() {
        if (clientHandle != nullptr) {
            WlanCloseHandle(clientHandle, nullptr);
        }
    }

    std::vector<NetworkInfo> scan() override {
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

                // Convert SSID to UTF-8
                std::string ssid = std::string(
                    reinterpret_cast<char*>(network.dot11Ssid.ucSSID),
                    network.dot11Ssid.uSSIDLength
                );
                info.ssid = ssid;
                info.signalStrength = static_cast<int>(network.wlanSignalQuality);
                
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

                // Get BSS list for additional information
                PWLAN_BSS_LIST bssList = nullptr;
                result = WlanGetNetworkBssList(
                    clientHandle,
                    &interfaceListPtr->InterfaceInfo[i].InterfaceGuid,
                    &network.dot11Ssid,
                    network.dot11BssType,
                    network.bSecurityEnabled,
                    nullptr,
                    &bssList
                );

                if (result == ERROR_SUCCESS && bssList && bssList->dwNumberOfItems > 0) {
                    char bssid[18];
                    snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X",
                            bssList->wlanBssEntries[0].dot11Bssid[0],
                            bssList->wlanBssEntries[0].dot11Bssid[1],
                            bssList->wlanBssEntries[0].dot11Bssid[2],
                            bssList->wlanBssEntries[0].dot11Bssid[3],
                            bssList->wlanBssEntries[0].dot11Bssid[4],
                            bssList->wlanBssEntries[0].dot11Bssid[5]);
                    info.bssid = bssid;
                    
                    // Get channel and frequency
                    info.channel = bssList->wlanBssEntries[0].ulChCenterFrequency > 5000 ? 
                                  (bssList->wlanBssEntries[0].ulChCenterFrequency - 5000) / 5 : 
                                  (bssList->wlanBssEntries[0].ulChCenterFrequency - 2407) / 5;
                    info.frequency = static_cast<int>(bssList->wlanBssEntries[0].ulChCenterFrequency);
                    
                    WlanFreeMemory(bssList);
                } else {
                    info.bssid = "";
                    info.channel = 0;
                    info.frequency = 0;
                }

                // Add to list only if not a duplicate
                auto it = std::find_if(networks.begin(), networks.end(), 
                                      [&info](const NetworkInfo& existingInfo) {
                                          return existingInfo.ssid == info.ssid;
                                      });
                
                if (it == networks.end()) {
                    networks.push_back(info);
                }
            }
        }

        return networks;
    }

    bool connect(const std::string& ssid, const std::string& password) override {
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

        GUID interfaceGuid = interfaceListPtr->InterfaceInfo[0].InterfaceGuid;

        // Create profile XML
        std::string profileXmlStr =
            "<?xml version=\"1.0\"?>"
            "<WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">"
            "<name>" + ssid + "</name>"
            "<SSIDConfig>"
            "<SSID>"
            "<name>" + ssid + "</name>"
            "</SSID>"
            "</SSIDConfig>"
            "<connectionType>ESS</connectionType>"
            "<connectionMode>auto</connectionMode>"
            "<MSM>"
            "<security>"
            "<authEncryption>" +
            (password.empty() ?
                std::string("<authentication>open</authentication><encryption>none</encryption>") :
                std::string("<authentication>WPA2PSK</authentication><encryption>AES</encryption>")) +
            "</authEncryption>";
            
        if (!password.empty()) {
            profileXmlStr += "<sharedKey>"
                            "<keyType>passPhrase</keyType>"
                            "<protected>false</protected>"
                            "<keyMaterial>" + password + "</keyMaterial>"
                            "</sharedKey>";
        }
            
        profileXmlStr += "<useOneX>false</useOneX>"
                        "</security>"
                        "</MSM>"
                        "</WLANProfile>";
                        
        std::wstring profileXml = utf8ToWide(profileXmlStr);

        // Set the profile
        DWORD reasonCode = 0;
        result = WlanSetProfile(
            clientHandle,
            &interfaceGuid,
            0,
            profileXml.c_str(),
            nullptr,
            TRUE,
            nullptr,
            &reasonCode
        );
        
        if (result != ERROR_SUCCESS) {
            Logger::getInstance().error("Failed to set connection profile, reason code: ", reasonCode);
            return false;
        }

        // Connect using the profile
        std::wstring profileName = utf8ToWide(ssid);
        WLAN_CONNECTION_PARAMETERS connectionParams = {};
        connectionParams.wlanConnectionMode = wlan_connection_mode_profile;
        connectionParams.strProfile = profileName.c_str();
        connectionParams.pDot11Ssid = nullptr;
        connectionParams.pDesiredBssidList = nullptr;
        connectionParams.dot11BssType = dot11_BSS_type_infrastructure;
        connectionParams.dwFlags = 0;
        
        result = WlanConnect(
            clientHandle,
            &interfaceGuid,
            &connectionParams,
            nullptr
        );
        
        if (result != ERROR_SUCCESS) {
            Logger::getInstance().error("Failed to connect to network");
            return false;
        }
        
        Logger::getInstance().info("Successfully connected to network: ", ssid);
        return true;
    }

    bool disconnect() override {
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

    ConnectionStatus getStatus() const override {
        PWLAN_INTERFACE_INFO_LIST interfaceList = nullptr;
        DWORD result = WlanEnumInterfaces(clientHandle, nullptr, &interfaceList);
        
        if (result != ERROR_SUCCESS) {
            Logger::getInstance().error("Failed to enumerate WLAN interfaces");
            return ConnectionStatus::CONNECTION_ERROR;
        }

        std::unique_ptr<WLAN_INTERFACE_INFO_LIST, decltype(&WlanFreeMemory)> 
            interfaceListPtr(interfaceList, WlanFreeMemory);

        if (interfaceListPtr->dwNumberOfItems == 0) {
            Logger::getInstance().error("No WLAN interfaces found");
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
                return ConnectionStatus::CONNECTING;
            default:
                return ConnectionStatus::CONNECTION_ERROR;
        }
    }

    // Hotspot functionality - simplified implementation for now
    bool createHotspot(const std::string& ssid) override {
        Logger::getInstance().warning("Hotspot creation is not yet supported on Windows platform");
        return false;
    }

    bool stopHotspot() override {
        Logger::getInstance().warning("Hotspot functionality is not yet supported on Windows platform");
        return false;
    }

    bool isHotspotActive() const override {
        return false;  // Not supported yet
    }

    bool isHotspotSupported() const override {
        return false;  // Not supported yet
    }

private:
    HANDLE clientHandle = nullptr;
};

// Factory function implementation for Windows
std::unique_ptr<WifiImpl> createPlatformImpl() {
    return std::make_unique<WindowsWifiImpl>();
}

} // namespace wificpp

#endif // WIFICPP_PLATFORM_WINDOWS
