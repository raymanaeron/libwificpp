#pragma once

#include "wifi_types.hpp"
#include <memory>
#include <string>
#include <vector>

namespace wificpp {

class WifiImpl;  // Forward declaration

class WifiManager {
public:
    WifiManager();
    ~WifiManager();

    // Prevent copy and assignment
    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;

    // WiFi operations
    std::vector<NetworkInfo> scan();
    bool connect(const std::string& ssid, const std::string& password = "");
    bool disconnect();
    ConnectionStatus getStatus() const;
    
    // Hotspot management
    bool createHotspot(const std::string& ssid);
    bool stopHotspot();
    bool isHotspotActive() const;
    bool isHotspotSupported() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace wificpp
