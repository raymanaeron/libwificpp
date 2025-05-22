#pragma once

#include <string>
#include <vector>
#include <memory>
#include "wifi_types.hpp"

namespace wificpp {

class WifiManager {
public:
    WifiManager();
    ~WifiManager();

    // Scan for available networks
    std::vector<NetworkInfo> scan();
    
    // Connect to a network
    bool connect(const std::string& ssid, const std::string& password = "");
    
    // Disconnect from current network
    bool disconnect();    // Get current connection status
    ConnectionStatus getStatus() const;
    
    // Hotspot management
    /**
     * Create an unsecured WiFi hotspot with the given SSID.
     * 
     * @param ssid The SSID (network name) for the hotspot
     * @return true if the hotspot was created successfully, false otherwise
     * @note This operation typically requires administrative privileges
     */
    bool createHotspot(const std::string& ssid);
    
    /**
     * Stop the active hotspot.
     * 
     * @return true if the hotspot was stopped successfully or if no hotspot was active, false otherwise
     */
    bool stopHotspot();
    
    /**
     * Check if a hotspot is currently active.
     * 
     * @return true if a hotspot is active, false otherwise
     */
    bool isHotspotActive() const;
    
    /**
     * Check if the hardware supports hotspot functionality.
     * 
     * @return true if the hardware supports creating hotspots, false otherwise
     */
    bool isHotspotSupported() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace wificpp
