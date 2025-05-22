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
#include <CoreLocation/CoreLocation.h>
#include <AppKit/AppKit.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <dispatch/dispatch.h>

// Helper interface for Location Services delegate
@interface LocationDelegate : NSObject <CLLocationManagerDelegate>
@property (nonatomic, strong) dispatch_semaphore_t authSemaphore;
@property (nonatomic, assign) BOOL isAuthorized;
@property (nonatomic, strong) dispatch_group_t initGroup;
@end

@implementation LocationDelegate

- (instancetype)init {
    if (self = [super init]) {
        _authSemaphore = dispatch_semaphore_create(0);
        _isAuthorized = NO;
        _initGroup = dispatch_group_create();
    }
    return self;
}

- (void)locationManager:(CLLocationManager *)manager didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
    // Log the authorization status change
    NSString* statusStr;
    switch (status) {
        case kCLAuthorizationStatusAuthorizedAlways:
            statusStr = @"AuthorizedAlways";
            break;
        case kCLAuthorizationStatusDenied:
            statusStr = @"Denied";
            break;
        case kCLAuthorizationStatusRestricted:
            statusStr = @"Restricted";
            break;
        case kCLAuthorizationStatusNotDetermined:
            statusStr = @"NotDetermined";
            break;
        default:
            statusStr = @"Unknown";
    }
    NSLog(@"Location authorization status changed to: %@", statusStr);
    
    // Update authorization state
    self.isAuthorized = (status == kCLAuthorizationStatusAuthorizedAlways);
    
    // Signal for any status change
    dispatch_semaphore_signal(_authSemaphore);
    dispatch_group_leave(_initGroup);
}

@end

namespace wificpp {

// Helper functions
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

std::string NSStringToStdString(NSString* nsString) {
    if (!nsString) return "";
    return std::string([nsString UTF8String]);
}

CFStringRef StdToCFString(const std::string& str) {
    return CFStringCreateWithCString(kCFAllocatorDefault, str.c_str(), kCFStringEncodingUTF8);
}

NSString* StdToNSString(const std::string& str) {
    return [NSString stringWithUTF8String:str.c_str()];
}

class MacOSWifiImpl : public WifiImpl {
public:
    MacOSWifiImpl() {
        // Initialize in an autorelease pool
        @autoreleasepool {
            // Initialize AppKit properly
            if (NSApp == nil) {
                [NSApplication sharedApplication];
            }
            [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
            
            // Initialize CoreWLAN interface
            wifiClient = [CWWiFiClient sharedWiFiClient];
            if (!wifiClient) {
                throw std::runtime_error("Failed to initialize CWWiFiClient");
            }
            
            wifiInterface = [wifiClient interface];
            if (!wifiInterface) {
                throw std::runtime_error("No WiFi interface found");
            }
            
            // Initialize Location Manager on main thread with proper error handling
            dispatch_sync(dispatch_get_main_queue(), ^{
                @autoreleasepool {
                    @try {
                        locationManager = [[CLLocationManager alloc] init];
                        if (!locationManager) {
                            NSLog(@"Failed to create CLLocationManager");
                            return;
                        }
                        
                        locationDelegate = [[LocationDelegate alloc] init];
                        if (!locationDelegate) {
                            NSLog(@"Failed to create LocationDelegate");
                            return;
                        }
                        
                        locationManager.delegate = locationDelegate;
                        
                        // Check initial authorization status
                        CLAuthorizationStatus status;
                        if (@available(macOS 11.0, *)) {
                            status = locationManager.authorizationStatus;
                        } else {
                            status = [CLLocationManager authorizationStatus];
                        }
                        
                        // Log initial status
                        switch (status) {
                            case kCLAuthorizationStatusNotDetermined:
                                NSLog(@"Location Services authorization status: Not Determined");
                                break;
                            case kCLAuthorizationStatusRestricted:
                                NSLog(@"Location Services authorization status: Restricted");
                                break;
                            case kCLAuthorizationStatusDenied:
                                NSLog(@"Location Services authorization status: Denied");
                                break;
                            case kCLAuthorizationStatusAuthorizedAlways:
                                NSLog(@"Location Services authorization status: Authorized Always");
                                break;
                            default:
                                NSLog(@"Location Services authorization status: Unknown");
                                break;
                        }
                    } @catch (NSException *exception) {
                        NSLog(@"Exception during Location Services initialization: %@", exception);
                    }
                }
            });
            
            // Get interface name
            interfaceName = NSStringToStdString([wifiInterface interfaceName]);
            Logger::getInstance().info("WifiManager initialized on macOS platform with interface " + interfaceName);
        }
    }
    
    ~MacOSWifiImpl() {
        @autoreleasepool {
            if (locationManager) {
                // Clean up location manager on main thread
                dispatch_sync(dispatch_get_main_queue(), ^{
                    @autoreleasepool {
                        locationManager.delegate = nil;
                        locationDelegate = nil;
                        locationManager = nil;
                    }
                });
            }
            
            // Clean up interface and client
            wifiInterface = nil;
            wifiClient = nil;
        }
    }
    
    std::vector<NetworkInfo> scan() override {
        std::vector<NetworkInfo> networks;
        Logger::getInstance().info("Scanning for networks on macOS interface " + interfaceName);

        // Ensure we're running with proper autorelease pool
        @autoreleasepool {
            // Check WiFi state first
            if (![wifiInterface powerOn]) {
                Logger::getInstance().error("WiFi is disabled. Please enable WiFi in System Settings.");
                return networks;
            }

            // Ensure Location Services are authorized
            if (!locationDelegate.isAuthorized) {
                Logger::getInstance().info("Requesting Location Services authorization...");
                
                // Request authorization synchronously on main thread
                dispatch_sync(dispatch_get_main_queue(), ^{
                    @autoreleasepool {
                        [NSApp activateIgnoringOtherApps:YES];
                        requestLocationServicesAuthorization();
                    }
                });
                
                if (locationDelegate.isAuthorized) {
                    Logger::getInstance().info("Location Services authorization granted");
                    // Small delay to let system process the authorization
                    [NSThread sleepForTimeInterval:1.0];
                } else {
                    Logger::getInstance().warning("Location Services authorization not granted. Network names may be limited.");
                }
            }

            // Attempt scanning with exponential backoff
            NSError* error = nil;
            NSSet<CWNetwork*>* scanResults = nil;
            int maxRetries = 5;
            double baseDelay = 0.5; // Start with 0.5 second delay
            
            for (int i = 0; i < maxRetries; i++) {
                @autoreleasepool {
                    // Check if a scan is already in progress
                    NSError* scanError = nil;
                    [wifiInterface scanForNetworksWithSSID:nil error:&scanError];
                    if (scanError && [scanError.domain isEqualToString:CWErrorDomain] && 
                        [scanError.localizedDescription containsString:@"busy"]) {
                        Logger::getInstance().info("A scan is already in progress, waiting...");
                        [NSThread sleepForTimeInterval:baseDelay * pow(2, i)];
                        continue;
                    }
                    
                    // Ensure we have a clean error object for each attempt
                    error = nil;
                    scanResults = nil;
                    
                    // Perform the scan synchronously since it's already in its own autorelease pool
                    NSDate* scanStartTime = [NSDate date];
                    scanResults = [wifiInterface scanForNetworksWithName:nil error:&error];
                    
                    // Log scan duration for debugging
                    NSTimeInterval scanDuration = -[scanStartTime timeIntervalSinceNow];
                    Logger::getInstance().info("Scan completed in " + std::to_string(scanDuration) + " seconds");
                    
                    if (error || !scanResults) {
                        NSString* errorStr = error ? [error localizedDescription] : @"Unknown error";
                        Logger::getInstance().error("Failed to scan for networks (attempt " + std::to_string(i+1) + "): " + 
                                                 NSStringToStdString(errorStr));
                        
                        // Handle specific error cases
                        if ([error.domain isEqualToString:CWErrorDomain]) {
                            if (error.code == kCWOperationNotPermittedErr) {
                                Logger::getInstance().error("WiFi scanning requires admin privileges or proper entitlements");
                                return networks;
                            }
                        }
                        
                        if (i < maxRetries - 1) continue;
                        return networks;
                    }

                    // Check for valid results
                    if (!scanResults || [scanResults count] == 0) {
                        Logger::getInstance().info("No networks found in scan");
                        if (i < maxRetries - 1) continue;
                        return networks;
                    }

                    // Verify we have network names
                    bool hasValidResults = false;
                    for (CWNetwork* network in scanResults) {
                        if ([network ssid]) {
                            hasValidResults = true;
                            break;
                        }
                    }

                    if (hasValidResults) {
                        Logger::getInstance().info("Successfully retrieved network names");
                        break;
                    } else if (i < maxRetries - 1) {
                        Logger::getInstance().info("No network names available yet, will retry...");
                        continue;
                    }
                }
            }

            // Process scan results
            for (CWNetwork* network in scanResults) {
                NetworkInfo info;
                
                // Get SSID
                NSString* ssidValue = [network ssid];
                info.ssid = ssidValue ? NSStringToStdString(ssidValue) : "[Hidden Network]";
                
                // Get BSSID
                NSString* bssidValue = [network bssid];
                info.bssid = bssidValue ? NSStringToStdString(bssidValue) : "[No Access]";
                
                // Get signal strength
                info.signalStrength = static_cast<int>([network rssiValue]);
                
                // Get channel and frequency
                CWChannel* channel = [network wlanChannel];
                if (channel) {
                    info.channel = static_cast<int>([channel channelNumber]);
                    
                    if ([channel channelBand] == kCWChannelBand2GHz) {
                        info.frequency = 2412 + ((info.channel - 1) * 5);
                    } else if ([channel channelBand] == kCWChannelBand5GHz) {
                        info.frequency = 5170 + ((info.channel - 34) * 5);
                    }
                }
                
                // Get security type
                info.security = SecurityType::UNKNOWN;
                if ([network supportsSecurity:kCWSecurityNone]) {
                    info.security = SecurityType::NONE;
                } else if ([network supportsSecurity:kCWSecurityWEP]) {
                    info.security = SecurityType::WEP;
                } else if ([network supportsSecurity:kCWSecurityWPA2Personal] ||
                         [network supportsSecurity:kCWSecurityWPA2Enterprise]) {
                    info.security = SecurityType::WPA2;
                } else if ([network supportsSecurity:kCWSecurityWPAPersonal] ||
                         [network supportsSecurity:kCWSecurityWPAEnterprise]) {
                    info.security = SecurityType::WPA;
                }
                
                networks.push_back(info);
            }
        }
        
        Logger::getInstance().info("Found " + std::to_string(networks.size()) + " networks");
        return networks;
    }
    
    bool connect(const std::string& ssid, const std::string& password) override {
        Logger::getInstance().info("Connecting to network: " + ssid);
        
        NSError* error = nil;
        NSString* nsSsid = StdToNSString(ssid);
        NSString* nsPassword = password.empty() ? nil : StdToNSString(password);
        
        NSSet<CWNetwork*>* scanResults = [wifiInterface scanForNetworksWithName:nsSsid error:&error];
        
        if (error || !scanResults || [scanResults count] == 0) {
            Logger::getInstance().error("Failed to find network: " + 
                                     (error ? NSStringToStdString([error localizedDescription]) : "Network not found"));
            return false;
        }
        
        CWNetwork* network = [scanResults anyObject];
        BOOL success = [wifiInterface associateToNetwork:network password:nsPassword error:&error];
        
        if (!success) {
            Logger::getInstance().error("Failed to connect to network: " + 
                                     (error ? NSStringToStdString([error localizedDescription]) : "Unknown error"));
            return false;
        }
        
        sleep(2);
        return getStatus() == ConnectionStatus::CONNECTED;
    }
    
    bool disconnect() override {
        Logger::getInstance().info("Disconnecting from network");
        
        [wifiInterface disassociate];
        sleep(1);
        
        return getStatus() == ConnectionStatus::DISCONNECTED;
    }
    
    ConnectionStatus getStatus() const override {
        if (![wifiInterface powerOn]) {
            return ConnectionStatus::DISCONNECTED;
        }
        
        NSString* currentSsid = [wifiInterface ssid];
        if (!currentSsid) {
            return ConnectionStatus::DISCONNECTED;
        }
        
        if (!hasIpAddress(interfaceName)) {
            return ConnectionStatus::CONNECTING;
        }
        
        return ConnectionStatus::CONNECTED;
    }
    
    bool createHotspot(const std::string& ssid, const std::string& password) override {
        Logger::getInstance().warning("Hotspot creation not yet implemented on macOS");
        return false;
    }
    
    bool stopHotspot() override {
        Logger::getInstance().warning("Hotspot functionality not yet implemented on macOS");
        return false;
    }
    
    bool isHotspotActive() const override {
        return false;
    }
    
    bool isHotspotSupported() const override {
        return true;
    }
    
private:
    CWWiFiClient* wifiClient = nullptr;
    CWInterface* wifiInterface = nullptr;
    std::string interfaceName;
    CLLocationManager* locationManager = nullptr;
    LocationDelegate* locationDelegate = nullptr;
    
    bool requestLocationServicesAuthorization() {
        // Must be called on main thread
        if (![NSThread isMainThread]) {
            __block bool result = false;
            dispatch_sync(dispatch_get_main_queue(), ^{
                result = requestLocationServicesAuthorization();
            });
            return result;
        }
        
        @autoreleasepool {
            // First check if Location Services is enabled system-wide
            if (![CLLocationManager locationServicesEnabled]) {
                Logger::getInstance().warning("Location Services is disabled in System Settings.\n"
                                           "Please enable Location Services in System Settings > "
                                           "Privacy & Security > Location Services");
                return false;
            }

            // Ensure NSApplication is ready for UI
            [NSApp activateIgnoringOtherApps:YES];

            // Check current authorization status using correct API based on OS version
            CLAuthorizationStatus currentStatus;
            if (@available(macOS 11.0, *)) {
                currentStatus = locationManager.authorizationStatus;
            } else {
                #pragma clang diagnostic push
                #pragma clang diagnostic ignored "-Wdeprecated-declarations"
                currentStatus = [CLLocationManager authorizationStatus];
                #pragma clang diagnostic pop
            }

            // If already authorized, return success
            if (currentStatus == kCLAuthorizationStatusAuthorizedAlways) {
                locationDelegate.isAuthorized = YES;
                return true;
            }

            // Reset authorization state
            locationDelegate.authSemaphore = dispatch_semaphore_create(0);
            locationDelegate.isAuthorized = NO;

            // Create dispatch group for coordination
            dispatch_group_t group = locationDelegate.initGroup;
            dispatch_group_enter(group);

            // Request authorization with UI focus
            [locationManager requestAlwaysAuthorization];

            // Wait for response with a timeout
            dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC);
            long result = dispatch_group_wait(group, timeout);
            
            if (result != 0) {
                Logger::getInstance().warning("Timeout waiting for Location Services authorization response");
            } else {
                Logger::getInstance().info("Received Location Services authorization response");
            }

            // Check final authorization state
            if (!locationDelegate.isAuthorized) {
                Logger::getInstance().warning("Location Services authorization was denied");
                return false;
            }

            Logger::getInstance().info("Location Services authorization granted");
            return true;
        }
    }

    bool isLocationServicesAuthorized() const {
        if (![CLLocationManager locationServicesEnabled]) {
            return false;
        }

        CLAuthorizationStatus status;
        if (@available(macOS 11.0, *)) {
            status = locationManager.authorizationStatus;
        } else {
            #pragma clang diagnostic push
            #pragma clang diagnostic ignored "-Wdeprecated-declarations"
            status = [CLLocationManager authorizationStatus];
            #pragma clang diagnostic pop
        }
        return (status == kCLAuthorizationStatusAuthorizedAlways);
    }
    
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

std::unique_ptr<WifiImpl> createPlatformImpl() {
    return std::make_unique<MacOSWifiImpl>();
}

} // namespace wificpp

#endif // WIFICPP_PLATFORM_MACOS
