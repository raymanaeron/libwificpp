# Linux Platform Setup for libwificpp

## Prerequisites

1. **Supported Distributions**
   - Ubuntu 18.04 LTS or newer
   - Debian 10 or newer
   - Fedora 30 or newer
   - Other distributions should work but are not officially tested

2. **Required Packages**
   - Development tools: build-essential, cmake, git
   - Wireless dependencies: libnl-3-dev, libnl-genl-3-dev, libnl-route-3-dev
   - Network tools: wireless-tools, wpasupplicant, hostapd (for hotspot functionality)

3. **Optional Packages**
   - dnsmasq (for hotspot functionality)
   - iw (for advanced wireless operations)

## Installation Steps

1. **Install System Dependencies**

   For Debian/Ubuntu:
   ```bash
   sudo apt update
   sudo apt install build-essential cmake git \
       libnl-3-dev libnl-genl-3-dev libnl-route-3-dev \
       wireless-tools wpasupplicant hostapd iw dnsmasq
   ```

   For Fedora:
   ```bash
   sudo dnf install gcc-c++ cmake git \
       libnl3-devel \
       wireless-tools wpa_supplicant hostapd iw dnsmasq
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

3. **Update Library Cache**
   ```bash
   sudo ldconfig
   ```

## Elevated Privileges Requirements

The WiFi management library requires elevated privileges for most operations. You have several options:

1. **Run your application as root**
   ```bash
   sudo your_application
   ```

2. **Use setuid**
   ```bash
   sudo chown root:root your_application
   sudo chmod u+s your_application
   ```

3. **Configure sudo for specific commands**
   - Create a file in `/etc/sudoers.d/` with specific permissions

4. **Use capabilities**
   ```bash
   sudo setcap cap_net_admin,cap_net_raw+ep your_application
   ```

## NetworkManager Integration

If your system uses NetworkManager, you may need to:

1. **Disable NetworkManager for specific interfaces**
   - Create a file in `/etc/NetworkManager/conf.d/` with:
     ```
     [keyfile]
     unmanaged-devices=interface-name:wlan0
     ```
   - Replace `wlan0` with your wireless interface name
   - Restart NetworkManager: `sudo systemctl restart NetworkManager`

2. **Alternative: Use NetworkManager D-Bus API**
   - The library can be configured to use NetworkManager's D-Bus API instead of direct configuration
   - Add the following to your application configuration:
     ```
     use_network_manager=true
     ```

## Troubleshooting

1. **Permission Denied**
   - Verify your application has the necessary permissions
   - Check system logs: `journalctl -xe`

2. **Interface Not Found**
   - List available interfaces: `ip link show`
   - Ensure WiFi hardware isn't blocked: `rfkill list`
   - Unblock WiFi if needed: `rfkill unblock wifi`

3. **Hotspot Issues**
   - Verify hostapd is installed and working: `sudo hostapd --version`
   - Check if your WiFi card supports AP mode: `iw list | grep "Supported interface modes" -A 10`

## Known Limitations

1. Some operations require specific kernel modules
2. Not all WiFi cards support all features (especially hotspot functionality)
3. WPA3 support depends on both hardware and driver support
4. Performance may vary based on driver quality
