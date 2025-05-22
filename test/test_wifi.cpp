#include <iostream>
#include <string>
#include "wifi_manager.hpp"

#ifdef WIFICPP_PLATFORM_MACOS
#import <AppKit/AppKit.h>

@interface WifiTestDelegate : NSObject<NSApplicationDelegate> {
    dispatch_semaphore_t _completionSemaphore;
}
@property (atomic) bool scanComplete;
@end

@implementation WifiTestDelegate

- (instancetype)init {
    if (self = [super init]) {
        _scanComplete = false;
        _completionSemaphore = dispatch_semaphore_create(0);
    }
    return self;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // Ensure we're properly initialized before starting
    if (!_completionSemaphore) {
        std::cerr << "Error: Semaphore not initialized\n";
        [NSApp terminate:nil];
        return;
    }

    // Schedule the WiFi operations to run after a short delay to ensure UI is ready
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.5 * NSEC_PER_SEC),
                  dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        @autoreleasepool {
            @try {
                std::cout << "Initializing WiFi manager...\n";
                wificpp::WifiManager wifi;
                
                std::cout << "Scanning for WiFi networks...\n"
                          << "Note: On macOS, network names require Location Services authorization.\n"
                          << "If prompted, please select 'Allow' to grant location access.\n"
                          << "If you see [Hidden Network] values, check System Settings > Privacy & Security > Location Services.\n\n";

                auto networks = wifi.scan();
                std::cout << "\nFound " << networks.size() << " networks:\n";
                std::cout << "----------------------------------------\n";
                
                for (const auto& network : networks) {
                    std::cout << "SSID: " << network.ssid << "\n"
                             << "BSSID: " << network.bssid << "\n"
                             << "Signal: " << network.signalStrength << "%\n"
                             << "Security: " << network.getSecurityString() << "\n"
                             << "Channel: " << network.channel << "\n"
                             << "Frequency: " << network.frequency << " MHz\n"
                             << "----------------------------------------\n";
                }

                // Clean up WiFi manager explicitly
                wifi = wificpp::WifiManager();
                self.scanComplete = true;
            } @catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
            } @finally {
                // Always signal completion, even on error
                dispatch_semaphore_signal(_completionSemaphore);
                dispatch_async(dispatch_get_main_queue(), ^{
                    [NSApp terminate:nil];
                });
            }
        }
    });
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    // Allow immediate termination if scan is complete
    return self.scanComplete ? NSTerminateNow : NSTerminateCancel;
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    // Final cleanup
    if (_completionSemaphore) {
        dispatch_semaphore_signal(_completionSemaphore);
    }
}

- (void)dealloc {
    if (_completionSemaphore) {
        dispatch_release(_completionSemaphore);
    }
}

@end
#endif

int main() {
#ifdef WIFICPP_PLATFORM_MACOS
    @autoreleasepool {
        // Get the shared application instance
        [NSApplication sharedApplication];
        
        // Set activation policy to accessory (no dock icon)
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        
        // Create menu bar (required for proper AppKit behavior)
        id menubar = [[NSMenu new] autorelease];
        id appMenuItem = [[NSMenuItem new] autorelease];
        [menubar addItem:appMenuItem];
        [NSApp setMainMenu:menubar];
        
        // Create application menu
        id appMenu = [[NSMenu new] autorelease];
        id quitMenuItem = [[[NSMenuItem alloc] initWithTitle:@"Quit"
                                                    action:@selector(terminate:)
                                             keyEquivalent:@"q"] autorelease];
        [appMenu addItem:quitMenuItem];
        [appMenuItem setSubmenu:appMenu];
        
        // Create and set up delegate
        WifiTestDelegate* delegate = [[[WifiTestDelegate alloc] init] autorelease];
        [NSApp setDelegate:delegate];
        
        // Activate the application
        [NSApp finishLaunching];
        [NSApp activateIgnoringOtherApps:YES];
        
        // Run with a timeout
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC);
            if (dispatch_semaphore_wait(delegate->_completionSemaphore, timeout) != 0) {
                std::cerr << "Timeout waiting for scan completion\n";
                [NSApp terminate:nil];
            }
        });
        
        [NSApp run];
    }
#else
    try {
        std::cout << "Initializing WiFi manager...\n";
        wificpp::WifiManager wifi;
        
        std::cout << "Scanning for WiFi networks...\n";
        auto networks = wifi.scan();
        std::cout << "\nFound " << networks.size() << " networks:\n";
        std::cout << "----------------------------------------\n";
        
        for (const auto& network : networks) {
            std::cout << "SSID: " << network.ssid << "\n"
                     << "BSSID: " << network.bssid << "\n"
                     << "Signal: " << network.signalStrength << "%\n"
                     << "Security: " << network.getSecurityString() << "\n"
                     << "Channel: " << network.channel << "\n"
                     << "Frequency: " << network.frequency << " MHz\n"
                     << "----------------------------------------\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
#endif
    
    return 0;
}
