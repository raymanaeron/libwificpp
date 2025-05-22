# macOS Platform Setup for libwificpp

## Prerequisites

1. **Supported macOS Versions**
   - macOS 10.13 (High Sierra) or newer
   - macOS 11.0 (Big Sur) or newer recommended for full functionality

2. **Required Tools**
   - Xcode Command Line Tools: `xcode-select --install`
   - Homebrew (recommended for easy dependency management): [Install Homebrew](https://brew.sh)
   - CMake 3.12 or newer: `brew install cmake`

3. **Required Frameworks**
   - CoreWLAN framework (included with macOS)
   - CoreFoundation framework (included with macOS)
   - SystemConfiguration framework (included with macOS)

## Installation Steps

1. **Install Development Tools**
   ```bash
   xcode-select --install
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   brew install cmake
   ```

2. **Build and Install libwificpp**
   ```bash
   git clone https://github.com/raymanaeron/libwificpp.git
   cd libwificpp
   mkdir build && cd build
   cmake ..
   make
   sudo make install
   ```

## Security and Permissions

macOS has strict security requirements for applications that access WiFi functionality:

1. **Application Entitlements**
   - Create an entitlements file for your application (`your_app.entitlements`):
     ```xml
     <?xml version="1.0" encoding="UTF-8"?>
     <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
     <plist version="1.0">
     <dict>
         <key>com.apple.developer.networking.wifi-info</key>
         <true/>
         <key>com.apple.developer.networking.HotspotHelper</key>
         <true/>
     </dict>
     </plist>
     ```

2. **Code Signing**
   - Applications must be properly signed to use WiFi capabilities:
     ```bash
     codesign --force --sign "Developer ID Application: Your Name (YOUR_TEAM_ID)" \
              --entitlements your_app.entitlements \
              your_application.app
     ```

3. **Privacy Permissions**
   - Add the following keys to your app's Info.plist:
     ```xml
     <key>NSLocationUsageDescription</key>
     <string>This app requires location access to scan for WiFi networks</string>
     <key>NSLocationAlwaysAndWhenInUseUsageDescription</key>
     <string>This app requires location access to scan for WiFi networks</string>
     <key>NSLocationWhenInUseUsageDescription</key>
     <string>This app requires location access to scan for WiFi networks</string>
     ```

## Integration in Xcode Projects

1. **Framework Integration**
   - Add libwificpp.dylib to your Xcode project
   - Add the required Apple frameworks:
     - CoreWLAN.framework
     - CoreFoundation.framework
     - SystemConfiguration.framework

2. **Header Search Paths**
   - Add the path to libwificpp headers: `/usr/local/include`

3. **Library Search Paths**
   - Add the path to libwificpp library: `/usr/local/lib`

## Troubleshooting

1. **WiFi Scanning Issues**
   - Ensure location services are enabled for your application
   - Check system privacy settings: System Preferences → Security & Privacy → Privacy → Location Services

2. **Permission Denied**
   - Verify your application has proper entitlements
   - Check console logs for permission issues: `Console.app`

3. **WiFi Service Not Available**
   - Ensure WiFi is enabled on the system
   - Check system integrity protection status: `csrutil status`

## Known Limitations

1. **Hotspot Creation**
   - Creating WiFi hotspots has additional restrictions on macOS
   - Full hotspot functionality may require a special entitlement from Apple

2. **WiFi Direct**
   - Limited support for WiFi Direct in the macOS API

3. **Monterey and Later**
   - macOS 12 (Monterey) and later may have additional restrictions
   - Some features may require user approval at runtime

## Enabling Location Services for WiFi Scanning

macOS requires Location Services to be enabled to access certain WiFi information, particularly SSID and BSSID data. Starting with macOS 10.15 (Catalina), Apple has restricted access to this data as a privacy measure.

### Understanding the Issue

When scanning for WiFi networks on macOS without proper Location Services permissions:
- SSID values will appear as `[Hidden Network]` instead of the actual network name
- BSSID values will appear as `[No Access]` instead of the MAC address
- Other network information (signal strength, channel, etc.) will still be available

This is a macOS security feature, not a bug in libwificpp. The library handles this situation by providing placeholder values when the system restricts access to this information.

### For Command Line Applications

1. **Enable Location Services System-Wide**:
   - Open System Preferences (or System Settings on macOS Ventura and newer)
   - Go to Privacy & Security → Location Services
   - Ensure the main "Location Services" checkbox is enabled
   - Find "Terminal" in the list of applications and check its box
   - If Terminal isn't listed, you may need to run your application once, which will prompt for permission

2. **Convert Command Line App to a macOS Bundle (recommended approach)**:
   
   Command line applications don't show proper permission dialogs by default. Convert your app to a bundle:
   
   ```bash
   # Create a basic app bundle structure
   mkdir -p MyWifiApp.app/Contents/MacOS
   mkdir -p MyWifiApp.app/Contents/Resources
   
   # Copy your executable
   cp /path/to/your/executable MyWifiApp.app/Contents/MacOS/
   
   # Create an Info.plist with location permissions
   cat > MyWifiApp.app/Contents/Info.plist << EOF
   <?xml version="1.0" encoding="UTF-8"?>
   <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
   <plist version="1.0">
   <dict>
       <key>CFBundleExecutable</key>
       <string>$(EXECUTABLE_NAME)</string>
       <key>CFBundleIdentifier</key>
       <string>com.example.mywifiapp</string>
       <key>CFBundleName</key>
       <string>MyWifiApp</string>
       <key>CFBundleVersion</key>
       <string>1.0</string>
       <key>NSLocationUsageDescription</key>
       <string>This app needs location access to scan WiFi networks</string>
       <key>NSLocationAlwaysAndWhenInUseUsageDescription</key>
       <string>This app needs location access to scan WiFi networks</string>
       <key>NSLocationWhenInUseUsageDescription</key>
       <string>This app needs location access to scan WiFi networks</string>
   </dict>
   </plist>
   EOF
   
   # Sign the app (even with a self-signed certificate)
   codesign --force --sign - MyWifiApp.app
   
   # Run your app (will trigger permission dialog)
   open MyWifiApp.app
   ```

3. **Reset Location Services Cache** (if needed):
   ```bash
   sudo launchctl unload /System/Library/LaunchDaemons/com.apple.locationd.plist
   sudo launchctl load /System/Library/LaunchDaemons/com.apple.locationd.plist
   ```

### For GUI Applications

1. **Create and Properly Sign Your Application**:
   - Include the required entitlements as described in the Security and Permissions section
   - Add the necessary privacy strings to Info.plist

2. **Request Location Permissions in Code**:
   - Add CoreLocation framework to your project
   - Request location permissions at runtime:
   ```objective-c
   // Objective-C
   #import <CoreLocation/CoreLocation.h>
   
   CLLocationManager *locationManager = [[CLLocationManager alloc] init];
   [locationManager requestWhenInUseAuthorization];
   ```
   or
   ```swift
   // Swift
   import CoreLocation
   
   let locationManager = CLLocationManager()
   locationManager.requestWhenInUseAuthorization()
   ```

3. **Run Once to Prompt for Permissions**:
   - The first time your application runs, the user will be prompted to allow location access
   - The user must choose "Allow" for WiFi scanning to work properly

### Troubleshooting WiFi Network Information Access

If you still see values like `[Hidden Network]` or `[No Access]` for SSID and BSSID:

1. **Verify Location Services Status**:
   ```bash
   # Check general location services status
   defaults read /var/db/locationd/clients.plist
   
   # Check location permissions as a normal user
   plutil -p ~/Library/Preferences/com.apple.locationd.plist
   
   # Check logs for location service denials
   log show --predicate 'subsystem == "com.apple.locationd"' --info --last 1h
   ```

2. **Check Application Authorization**:
   - Open System Preferences → Privacy & Security → Location Services
   - Verify your application is listed and enabled
   - For bundled apps, make sure the bundle identifier in Info.plist matches what you expect
   - For terminal apps, make sure Terminal itself has location permission

3. **Reset Location Database** (requires admin privileges):
   ```bash
   # Reset location services entirely (use with caution)
   sudo rm /var/db/locationd/clients.plist
   sudo launchctl stop com.apple.locationd
   sudo launchctl start com.apple.locationd
   ```

4. **Create a Test Bundle App**:
   Sometimes the easiest solution is to create a proper app bundle with the correct entitlements:
   
   ```bash
   # Create a simple script that runs your test app
   cat > run_wifi_test.sh << EOF
   #!/bin/bash
   cd "$(dirname "$0")"
   exec ./test_wifi
   EOF
   
   chmod +x run_wifi_test.sh
   
   # Create bundle structure
   mkdir -p WiFiTest.app/Contents/MacOS
   mkdir -p WiFiTest.app/Contents/Resources
   
   # Copy the executable and script
   cp run_wifi_test.sh WiFiTest.app/Contents/MacOS/WiFiTest
   cp test_wifi WiFiTest.app/Contents/MacOS/
   
   # Create Info.plist
   cat > WiFiTest.app/Contents/Info.plist << EOF
   <?xml version="1.0" encoding="UTF-8"?>
   <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
   <plist version="1.0">
   <dict>
       <key>CFBundleExecutable</key>
       <string>WiFiTest</string>
       <key>CFBundleIdentifier</key>
       <string>com.example.wifitest</string>
       <key>CFBundleName</key>
       <string>WiFiTest</string>
       <key>CFBundleVersion</key>
       <string>1.0</string>
       <key>NSLocationUsageDescription</key>
       <string>This app needs to access your location to scan WiFi networks</string>
       <key>NSLocationAlwaysAndWhenInUseUsageDescription</key>
       <string>This app needs to access your location to scan WiFi networks</string>
       <key>NSLocationWhenInUseUsageDescription</key>
       <string>This app needs to access your location to scan WiFi networks</string>
   </dict>
   </plist>
   EOF
   
   # Sign and run
   codesign --force --sign - WiFiTest.app
   open WiFiTest.app
   ```

### Technical Details

The CoreWLAN framework on macOS may return `nil` for the `ssid` and `bssid` properties of `CWNetwork` objects when Location Services permissions are not granted. This is not a bug but a privacy feature implemented by Apple.

The libwificpp library handles this by:
1. Checking if Location Services is authorized during WiFi scanning
2. Attempting to request permissions if not already granted
3. Providing placeholder values (`[Hidden Network]` for SSID and `[No Access]` for BSSID) when access is restricted
4. Still providing all other available network information

Even with Location Services enabled, some networks may still show as `[Hidden Network]` if they are configured to not broadcast their SSID.

### Testing Your Setup

To verify that your Location Services setup is working correctly:

1. Run the test application:
   ```bash
   cd ~/code/libwificpp/build
   ./test_wifi
   ```

2. Check the output for SSID and BSSID values:
   - If you see actual network names and MAC addresses, Location Services is configured correctly
   - If you see `[Hidden Network]` and `[No Access]`, follow the troubleshooting steps above
