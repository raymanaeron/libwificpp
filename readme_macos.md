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
