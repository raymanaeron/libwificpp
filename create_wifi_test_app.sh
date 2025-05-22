#!/bin/bash
# create_wifi_test_app.sh
# This script creates a macOS app bundle for testing WiFi scanning with location services

echo "====================================================================="
echo "WiFi Test App Creator for macOS Location Services Testing"
echo "====================================================================="

echo "Creating WiFi Test application bundle with Location Services permissions..."

# Check if we have test_wifi executable
if [ ! -f "build/test_wifi" ]; then
  echo "Building test_wifi application first..."
  mkdir -p build
  cd build
  cmake ..
  make
  cd ..
fi

# Create app bundle structure
echo "Creating bundle structure..."
mkdir -p WiFiTest.app/Contents/MacOS
mkdir -p WiFiTest.app/Contents/Resources

# Create Info.plist with Location Services requirements
cat > WiFiTest.app/Contents/Info.plist << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleIdentifier</key>
    <string>com.wificpp.test</string>
    <key>CFBundleName</key>
    <string>WiFiTest</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleExecutable</key>
    <string>WiFiTest</string>
    <key>NSLocationUsageDescription</key>
    <string>WiFi scanning requires access to Location Services to show network names on macOS</string>
    <key>NSLocationAlwaysAndWhenInUseUsageDescription</key>
    <string>WiFi scanning requires access to Location Services to show network names on macOS</string>
    <key>NSLocationWhenInUseUsageDescription</key>
    <string>WiFi scanning requires access to Location Services to show network names on macOS</string>
</dict>
</plist>
EOF

# Create launcher script that runs test_wifi
cat > WiFiTest.app/Contents/MacOS/WiFiTest << EOF
#!/bin/bash
cd "\$(dirname "\$0")"
open -a Terminal ./run_test_with_output.sh
EOF

# Create the script that will actually run the test and show output
cat > WiFiTest.app/Contents/MacOS/run_test_with_output.sh << EOF
#!/bin/bash
cd "\$(dirname "\$0")"
echo "====================================================================="
echo "Running WiFi Test with Location Services permissions..."
echo "====================================================================="
echo "Current Location Services Status:"
defaults read /var/db/locationd/clients.plist 2>/dev/null || echo "Unable to read location services status (this is normal)"
echo
echo "Initial WiFi Scanning Results (before Location Services are fully active):"
echo "------------------------------------------------------"
./test_wifi
echo "------------------------------------------------------"
echo
echo "Waiting 15 seconds for Location Services to become fully active..."
echo "(This delay is necessary for macOS to properly enable permissions)"
echo
for i in {15..1}; do
    echo -ne "\rTime remaining: \$i seconds..."
    sleep 1
done
echo -ne "\r\033[K"  # Clear the countdown line

echo "Second WiFi Scanning Results (after waiting for Location Services):"
echo "------------------------------------------------------"
./test_wifi
echo "------------------------------------------------------"
echo
if [ ! -f "/var/db/locationd/consent.plist" ]; then
    echo "Location Services consent file not found."
    echo "You may need to enable Location Services in System Preferences."
    echo "1. Go to System Preferences → Privacy & Security → Location Services"
    echo "2. Ensure WiFiTest.app is listed and enabled"
    echo "3. Try running the test again"
fi
echo "3. Try running the test again after enabling permissions"
echo "====================================================================="
echo
echo "Waiting for Location Services authorization (15 seconds)..."
echo "This delay allows time for the system to process the permission..."
for i in {15..1}; do
    echo -ne "\rStarting scan in \$i seconds..."
    sleep 1
done
echo -e "\n"

echo "WiFi Scanning Results:"
echo "------------------------------------------------------"
./test_wifi
echo "------------------------------------------------------"
echo
echo "If you don't see network names, trying one more time..."
sleep 2
./test_wifi
echo "------------------------------------------------------"
echo
echo "Test complete."
echo "* Empty SSID/BSSID values mean Location Services access is needed"
echo "* [Hidden Network]/[No Access] means Location Services was denied"
echo "* Normal SSID/BSSID values mean Location Services is working"
echo "====================================================================="
echo "Press any key to exit..."
read -n 1
EOF

# Make both scripts executable
chmod +x WiFiTest.app/Contents/MacOS/WiFiTest
chmod +x WiFiTest.app/Contents/MacOS/run_test_with_output.sh

# Copy the test_wifi executable
cp build/test_wifi WiFiTest.app/Contents/MacOS/

# Create Info.plist with location services permission
cat > WiFiTest.app/Contents/Info.plist << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>WiFiTest</string>
    <key>CFBundleIdentifier</key>
    <string>com.wificpp.wifitest</string>
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

# Sign the app
echo "Signing the application bundle..."
codesign --force --sign - WiFiTest.app

echo "====================================================================="
echo "Application bundle created successfully!"
echo "====================================================================="
echo "TO RUN THE TEST:"
echo "1. Type:    open WiFiTest.app"
echo "2. When prompted, select 'Allow' to grant location permissions"
echo "3. A Terminal window will open showing the WiFi scanning results"
echo "4. If you still see [Hidden Network] values, check System Preferences"
echo "   → Privacy & Security → Location Services to ensure WiFiTest is enabled"
echo "====================================================================="
echo
echo "NOTE: You'll need to run this test twice to see the difference:"
echo "- First run: Will show placeholder values while requesting permissions"
echo "- Second run: Should show actual network names if permission was granted"
echo "====================================================================="
