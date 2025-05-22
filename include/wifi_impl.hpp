#pragma once

#include "wifi_types.hpp"
#include "wifi_platform.hpp"
#include <memory>
#include <string>
#include <vector>

namespace wificpp {

// Implementation interface for platform-specific WiFi operations
class WifiImpl {
public:
    virtual ~WifiImpl() = default;
    
    // WiFi operations
    virtual std::vector<NetworkInfo> scan() = 0;
    virtual bool connect(const std::string& ssid, const std::string& password) = 0;
    virtual bool disconnect() = 0;
    virtual ConnectionStatus getStatus() const = 0;
      // Hotspot operations
    virtual bool createHotspot(const std::string& ssid, const std::string& password) = 0;
    virtual bool stopHotspot() = 0;
    virtual bool isHotspotActive() const = 0;
    virtual bool isHotspotSupported() const = 0;
};

// Factory function to create platform-specific implementation
std::unique_ptr<WifiImpl> createPlatformImpl();

} // namespace wificpp
