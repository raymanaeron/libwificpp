#!/bin/bash
cd "$(dirname "$0")"
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
    echo -ne "\rTime remaining: $i seconds..."
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
    echo -ne "\rStarting scan in $i seconds..."
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
