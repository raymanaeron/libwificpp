# iOS Platform Setup for libwificpp

## Important Limitations

Apple imposes strict limitations on WiFi functionality for third-party iOS apps:

1. **No Direct WiFi Control**: Apps cannot directly scan for WiFi networks or programmatically connect to networks without user intervention through Settings.

2. **API Restrictions**: Public APIs provide very limited functionality:
   - Can only retrieve information about the currently connected WiFi network
   - Cannot scan for available networks
   - Can join known networks with user permission (iOS 11+)

3. **App Store Compliance**: Using private APIs to access WiFi functionality will result in App Store rejection.

## Prerequisites

1. **Supported iOS Versions**
   - iOS 11.0 or newer for basic functionality (NEHotspotConfiguration)
   - iOS 13.0 or newer recommended

2. **Development Environment**
   - Xcode 11.0 or newer
   - CocoaPods or Swift Package Manager for dependency management

3. **Required Frameworks**
   - NetworkExtension.framework
   - SystemConfiguration.framework
   - CoreLocation.framework (for location permissions)

## Integration Steps

1. **Project Setup with CocoaPods**
   - Create a `Podfile`:
     ```ruby
     platform :ios, '11.0'
     
     target 'YourApp' do
       pod 'libwificpp', :path => '/path/to/libwificpp'
     end
     ```
   - Run `pod install`

2. **Manual Integration**
   - Add libwificpp.a or libwificpp.framework to your Xcode project
   - Add required frameworks:
     - NetworkExtension.framework
     - SystemConfiguration.framework
     - CoreLocation.framework

## Required Entitlements and Permissions

1. **Entitlements File (YourApp.entitlements)**
   ```xml
   <?xml version="1.0" encoding="UTF-8"?>
   <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
   <plist version="1.0">
   <dict>
       <key>com.apple.developer.networking.HotspotHelper</key>
       <true/>
       <key>com.apple.developer.networking.networkextension</key>
       <array>
           <string>hotspot-configure</string>
       </array>
   </dict>
   </plist>
   ```

2. **Info.plist Permissions**
   ```xml
   <key>NSLocationAlwaysAndWhenInUseUsageDescription</key>
   <string>This app requires location access to retrieve WiFi information</string>
   <key>NSLocationWhenInUseUsageDescription</key>
   <string>This app requires location access to retrieve WiFi information</string>
   ```

## Special Considerations

1. **Hotspot Configuration API**
   - Limited to iOS 11+
   - Requires special entitlements
   - Users still need to confirm connections via a system dialog

2. **Location Services**
   - Must be enabled to access current WiFi information
   - Requires explicit user permission

3. **Enterprise Deployment**
   - More WiFi capabilities available for enterprise-deployed apps
   - Managed configurations via MDM solutions

## Sample Integration Code

```swift
import NetworkExtension

// Request location permission (required for WiFi access)
let locationManager = CLLocationManager()
locationManager.requestWhenInUseAuthorization()

// Connect to WiFi network (iOS 11+)
func connectToNetwork(ssid: String, password: String) {
    let configuration = NEHotspotConfiguration(ssid: ssid, passphrase: password, isWEP: false)
    NEHotspotConfigurationManager.shared.apply(configuration) { error in
        if let error = error {
            print("Error connecting to WiFi: \(error.localizedDescription)")
        } else {
            print("Connected to \(ssid) successfully")
        }
    }
}

// Get current WiFi info
func getCurrentWiFiInfo() -> (ssid: String?, bssid: String?) {
    guard let interfaces = CNCopySupportedInterfaces() as? [String] else {
        return (nil, nil)
    }
    
    for interface in interfaces {
        if let interfaceInfo = CNCopyCurrentNetworkInfo(interface as CFString) as? [String: Any] {
            return (interfaceInfo["SSID"] as? String, interfaceInfo["BSSID"] as? String)
        }
    }
    
    return (nil, nil)
}
```

## Troubleshooting

1. **Unable to Access WiFi Information**
   - Check location permissions are approved
   - Ensure location services are enabled
   - Verify WiFi is enabled on the device

2. **NEHotspotConfiguration Fails**
   - Verify entitlements are correctly configured
   - Check that your provisioning profile includes the required entitlements
   - Ensure the network is in range

3. **App Store Rejection**
   - Ensure your app doesn't use any private APIs
   - Provide clear justification for using location services
   - Make sure entitlements are properly set up

## Known Limitations

1. **No Programmatic WiFi Scanning**
   - iOS prohibits apps from scanning available WiFi networks

2. **Limited Connection Capabilities**
   - Can only join networks with user confirmation
   - Cannot force disconnect from networks

3. **No Hotspot Creation**
   - Personal Hotspot creation/management is not available to third-party apps

4. **Enterprise Only Features**
   - Some WiFi management features require enterprise deployment
