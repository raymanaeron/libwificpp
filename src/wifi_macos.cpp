#include "wifi_impl.hpp"
#include "wifi_logger.hpp"
#include "wifi_platform.hpp"
#include "wifi_types.hpp"
#include <memory>
#include <stdexcept>
#include <vector>
#include <string>

#ifdef WIFICPP_PLATFORM_MACOS

// macOS specific headers
#include <CoreFoundation/CoreFoundation.h>
#include <CoreWLAN/CoreWLAN.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <net/if.h>
#include <ifaddrs.h>

namespace wificpp {

// Helper to convert CF types to std::string
std::string CFToStdString(CFStringRef cfString) {
    if (!cfString) return "";
    
    CFIndex length = CFStringGetLength(cfString);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    std::vector<char> buffer(maxSize, 0);
    
    if (CFStringGetCString(cfString, buffer.data(), maxSize, kCFStringEncodingUTF8)) {
        return std::string(buffer.data());
    }
    
    return "";
}

// Helper to convert std::string to CFStringRef
CFStringRef StdToCFString(const std::string& str) {
    return CFStringCreateWithCString(kCFAllocatorDefault, str.c_str(), kCFStringEncodingUTF8);
}

class MacOSWifiImpl : public WifiImpl {
public:
    MacOSWifiImpl() {
        // Initialize CoreWLAN interface
        wifiClient = CWWiFiClient::sharedWiFiClient();
        if (!wifiClient) {
            throw std::runtime_error("Failed to initialize CWWiFiClient");
        }
        
        // Get the default WiFi interface
        wifiInterface = wifiClient->interface();
        if (!wifiInterface) {
            throw std::runtime_error("No WiFi interface found");
        }
        
        interfaceName = CFToStdString(wifiInterface->interfaceName());
        Logger::getInstance().info("WifiManager initialized on macOS platform with interface " + interfaceName);
    }
    
    ~MacOSWifiImpl() {
        // CoreFoundation objects are reference counted and will be released automatically
    }
    
    std::vector<NetworkInfo> scan() override {
        std::vector<NetworkInfo> networks;
        Logger::getInstance().info("Scanning for networks on macOS interface " + interfaceName);
        
        NSError* error = nil;
        NSArray* scanResults = wifiInterface->scanForNetworksWithName(nil, error);
        
        if (error || !scanResults) {
            Logger::getInstance().error("Failed to scan for networks: " + 
                                       (error ? CFToStdString(error->localizedDescription()) : "Unknown error"));
            return networks;
        }
        
        // Convert scan results to NetworkInfo objects
        for (CWNetwork* network in scanResults) {
            NetworkInfo info;
            
            // Get SSID
            info.ssid = CFToStdString(network->ssid());
            
            // Get BSSID
            info.bssid = CFToStdString(network->bssid());
            
            // Get signal strength
            info.signalStrength = static_cast<int>(network->rssiValue());
            
            // Get channel and frequency
            CWChannel* channel = network->wlanChannel();
            if (channel) {
                info.channel = static_cast<int>(channel->channelNumber());
                
                // In macOS, you get the band (which indicates frequency range)
                if (channel->channelBand() == kCWChannelBand2GHz) {
                    // Calculate approximate frequency for 2.4GHz band
                    info.frequency = 2412 + ((info.channel - 1) * 5);
                } else if (channel->channelBand() == kCWChannelBand5GHz) {
                    // Calculate approximate frequency for 5GHz band
                    info.frequency = 5170 + ((info.channel - 34) * 5);
                }
            }
            
            // Get security type
            switch (network->security()) {
                case kCWSecurityNone:
                    info.security = SecurityType::NONE;
                    break;
                case kCWSecurityWEP:
                    info.security = SecurityType::WEP;
                    break;
                case kCWSecurityWPAPersonal:
                    info.security = SecurityType::WPA;
                    break;
                case kCWSecurityWPA2Personal:
                case kCWSecurityWPA2Enterprise:
                    info.security = SecurityType::WPA2;
                    break;
                default:
                    info.security = SecurityType::UNKNOWN;
            }
            
            networks.push_back(info);
        }
        
        Logger::getInstance().info("Found " + std::to_string(networks.size()) + " networks");
        return networks;
    }
    
    bool connect(const std::string& ssid, const std::string& password) override {
        Logger::getInstance().info("Connecting to network: " + ssid);
        
        NSError* error = nil;
        CFStringRef cfSsid = StdToCFString(ssid);
        CFStringRef cfPassword = StdToCFString(password);
        
        bool success = false;
        if (password.empty()) {
            // Connect to open network
            success = wifiInterface->associateToNetwork(
                CWNetwork::networkWithSSID(cfSsid, error),
                error
            );
        } else {
            // Create a password entry in keychain
            success = wifiInterface->associateToNetwork(
                CWNetwork::networkWithSSID(cfSsid, error),
                cfPassword,
                error
            );
        }
        
        CFRelease(cfSsid);
        CFRelease(cfPassword);
        
        if (!success) {
            Logger::getInstance().error("Failed to connect to network: " + 
                                       (error ? CFToStdString(error->localizedDescription()) : "Unknown error"));
            return false;
        }
        
        // Wait for connection to establish
        sleep(2);
        
        // Verify connection
        return getStatus() == ConnectionStatus::CONNECTED;
    }
    
    bool disconnect() override {
        Logger::getInstance().info("Disconnecting from network");
        
        NSError* error = nil;
        bool success = wifiInterface->disassociate();
        
        if (!success) {
            Logger::getInstance().error("Failed to disconnect: " + 
                                       (error ? CFToStdString(error->localizedDescription()) : "Unknown error"));
            return false;
        }
        
        return true;
    }
    
    ConnectionStatus getStatus() const override {
        // Check if WiFi is powered on
        if (!wifiInterface->powerOn()) {
            return ConnectionStatus::DISCONNECTED;
        }
        
        // Check for current network
        CWNetwork* currentNetwork = wifiInterface->currentNetwork();
        if (!currentNetwork) {
            return ConnectionStatus::DISCONNECTED;
        }
        
        // Check if we have an IP address
        if (!hasIpAddress(interfaceName)) {
            return ConnectionStatus::CONNECTING;
        }
        
        return ConnectionStatus::CONNECTED;
    }
    
    bool createHotspot(const std::string& ssid, const std::string& password = "") override {
        Logger::getInstance().warning("Hotspot creation not yet implemented on macOS");
        return false;
        
        // Note: macOS does have a programmatic way to create hotspots using the CWMutableConfiguration
        // class and applying it with CWInterface's startIBSSModeWithConfiguration method,
        // but it requires elevated privileges and is more complex than this example shows
    }
    
    bool stopHotspot() override {
        Logger::getInstance().warning("Hotspot functionality not yet implemented on macOS");
        return false;
    }
    
    bool isHotspotActive() const override {
        return false;  // Not implemented yet
    }
    
    bool isHotspotSupported() const override {
        // macOS does support hotspots (called "Computer to Computer networks" in macOS)
        return true;
    }
    
private:
    CWWiFiClient* wifiClient = nullptr;
    CWInterface* wifiInterface = nullptr;
    std::string interfaceName;
    
    // Helper method to check if an interface has an IP address
    bool hasIpAddress(const std::string& interface_name) const {
        struct ifaddrs* ifaddr;
        if (getifaddrs(&ifaddr) == -1) {
            return false;
        }
        
        bool has_ip = false;
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) continue;
            
            if (strcmp(ifa->ifa_name, interface_name.c_str()) == 0 && 
                ifa->ifa_addr->sa_family == AF_INET) {
                has_ip = true;
                break;
            }
        }
        
        freeifaddrs(ifaddr);
        return has_ip;
    }
};

// Factory function implementation for macOS
std::unique_ptr<WifiImpl> createPlatformImpl() {
    return std::make_unique<MacOSWifiImpl>();
}

} // namespace wificpp

#endif // WIFICPP_PLATFORM_MACOS
