#include "wifi_impl.hpp"
#include "wifi_logger.hpp"
#include "wifi_platform.hpp"
#include "wifi_types.hpp"
#include <memory>
#include <stdexcept>
#include <vector>
#include <string>

#ifdef WIFICPP_PLATFORM_IOS

#import <Foundation/Foundation.h>
#import <NetworkExtension/NetworkExtension.h>
#import <SystemConfiguration/CaptiveNetwork.h>
#import <CoreLocation/CoreLocation.h>

namespace wificpp {

// Helper to convert NSString to std::string
std::string NSStringToStdString(NSString* nsString) {
    if (!nsString) return "";
    return std::string([nsString UTF8String]);
}

// Helper to convert std::string to NSString
NSString* StdStringToNSString(const std::string& stdString) {
    return [NSString stringWithUTF8String:stdString.c_str()];
}

class IOSWifiImpl : public WifiImpl {
public:
    IOSWifiImpl() {
        Logger::getInstance().info("WifiManager initialized on iOS platform");
        
        // Check if NEHotspotConfiguration is available (iOS 11+)
        hotspotApiAvailable = (NSClassFromString(@"NEHotspotConfiguration") != nil);
        
        // For network scanning, we'll need to check for specific entitlements
        scanningAvailable = checkForScanningCapability();
    }
    
    ~IOSWifiImpl() {
        // Cleanup
    }
    
    std::vector<NetworkInfo> scan() override {
        std::vector<NetworkInfo> networks;
        Logger::getInstance().info("Scanning for networks on iOS");
        
        if (!scanningAvailable) {
            Logger::getInstance().warning("WiFi scanning is limited on iOS to currently connected network only");
        }
        
        // Use public API (limited, only returns current network)
        NSArray *interfaces = (__bridge_transfer NSArray *)CNCopySupportedInterfaces();
        if (interfaces) {
            for (NSString *interface in interfaces) {
                NSDictionary *info = (__bridge_transfer NSDictionary *)
                    CNCopyCurrentNetworkInfo((__bridge CFStringRef)interface);
                
                if (info && info[@"SSID"]) {
                    NetworkInfo network;
                    network.ssid = NSStringToStdString(info[@"SSID"]);
                    network.bssid = NSStringToStdString(info[@"BSSID"]);
                    
                    // Security type is not provided by the API
                    network.security = SecurityType::UNKNOWN;
                    
                    // Signal strength not available
                    network.signalStrength = 0;
                    
                    networks.push_back(network);
                }
            }
        }
        
        return networks;
    }
    
    bool connect(const std::string& ssid, const std::string& password) override {
        Logger::getInstance().info("Connecting to network: " + ssid);
        
        if (!hotspotApiAvailable) {
            Logger::getInstance().warning("NEHotspotConfiguration API not available (requires iOS 11+)");
            return false;
        }
        
        NSString *nsSSID = StdStringToNSString(ssid);
        NSString *nsPassword = StdStringToNSString(password);
        
        __block bool success = false;
        __block dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        
        NEHotspotConfiguration *config;
        if (password.empty()) {
            config = [[NEHotspotConfiguration alloc] initWithSSID:nsSSID];
        } else {
            config = [[NEHotspotConfiguration alloc] initWithSSID:nsSSID passphrase:nsPassword isWEP:NO];
        }
        
        [[NEHotspotConfigurationManager sharedManager] applyConfiguration:config completionHandler:^(NSError *error) {
            if (error) {
                NSLog(@"Error connecting to WiFi: %@", error);
                success = false;
            } else {
                success = true;
            }
            dispatch_semaphore_signal(semaphore);
        }];
        
        // Wait for callback (with timeout)
        dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 15 * NSEC_PER_SEC);
        dispatch_semaphore_wait(semaphore, timeout);
        
        return success;
    }
    
    bool disconnect() override {
        Logger::getInstance().info("Disconnecting from network");
        
        if (!hotspotApiAvailable) {
            Logger::getInstance().warning("NEHotspotConfiguration API not available (requires iOS 11+)");
            return false;
        }
        
        // Get current network SSID
        NSString *currentSSID = getCurrentSSID();
        if (!currentSSID) {
            Logger::getInstance().error("Unable to determine current network SSID");
            return false;
        }
        
        // Remove configuration
        [[NEHotspotConfigurationManager sharedManager] removeConfigurationForSSID:currentSSID];
        
        // Note: This doesn't actually disconnect - it just removes the configuration
        // iOS doesn't provide a public API to force disconnection
        
        return true;
    }
    
    ConnectionStatus getStatus() const override {
        // Check if we can get current WiFi info
        NSString *ssid = getCurrentSSID();
        
        if (ssid) {
            return ConnectionStatus::CONNECTED;
        } else {
            // Unfortunately, iOS doesn't easily expose connection state
            // We can only check if we're connected or not
            return ConnectionStatus::DISCONNECTED;
        }
    }
    
    bool createHotspot(const std::string& ssid, const std::string& password = "") override {
        Logger::getInstance().warning("Personal Hotspot functionality is not available via public APIs on iOS");
        return false;
    }
    
    bool stopHotspot() override {
        Logger::getInstance().warning("Personal Hotspot functionality is not available via public APIs on iOS");
        return false;
    }
    
    bool isHotspotActive() const override {
        return false;  // Not supported via public APIs
    }
    
    bool isHotspotSupported() const override {
        return false;  // Not supported via public APIs
    }
    
private:
    bool hotspotApiAvailable = false;
    bool scanningAvailable = false;
    
    bool checkForScanningCapability() const {
        // Check if we have location permissions which are required for WiFi scanning
        if (@available(iOS 13.0, *)) {
            CLAuthorizationStatus status = [CLLocationManager authorizationStatus];
            return (status == kCLAuthorizationStatusAuthorizedAlways || 
                    status == kCLAuthorizationStatusAuthorizedWhenInUse);
        }
        return false;
    }
    
    NSString* getCurrentSSID() const {
        NSArray *interfaces = (__bridge_transfer NSArray *)CNCopySupportedInterfaces();
        if (interfaces) {
            for (NSString *interface in interfaces) {
                NSDictionary *info = (__bridge_transfer NSDictionary *)
                    CNCopyCurrentNetworkInfo((__bridge CFStringRef)interface);
                
                if (info && info[@"SSID"]) {
                    return info[@"SSID"];
                }
            }
        }
        return nil;
    }
};

// Factory function implementation for iOS
std::unique_ptr<WifiImpl> createPlatformImpl() {
    return std::make_unique<IOSWifiImpl>();
}

} // namespace wificpp

#endif // WIFICPP_PLATFORM_IOS
