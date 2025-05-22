#include "wifi_manager.hpp"
#include "wifi_logger.hpp"
#include "wifi_impl.hpp"
#include "wifi_types.hpp"
#include <memory>
#include <stdexcept>

namespace wificpp {

// Forward declare the implementation
class WifiImpl;

// Implementation class definition using the platform abstraction
class WifiManager::Impl {
public:
    Impl() : platformImpl(createPlatformImpl()) {
        if (!platformImpl) {
            throw std::runtime_error("Failed to create platform implementation");
        }
        Logger::getInstance().info("WifiManager initialized");
    }

    ~Impl() = default;

    std::vector<NetworkInfo> scan() {
        return platformImpl->scan();
    }

    bool connect(const std::string& ssid, const std::string& password) {
        return platformImpl->connect(ssid, password);
    }

    bool disconnect() {
        return platformImpl->disconnect();
    }

    ConnectionStatus getStatus() const {
        return platformImpl->getStatus();
    }    bool createHotspot(const std::string& ssid, const std::string& password) {
        return platformImpl->createHotspot(ssid, password);
    }
    
    bool stopHotspot() {
        return platformImpl->stopHotspot();
    }
    
    bool isHotspotActive() const {
        return platformImpl->isHotspotActive();
    }
    
    bool isHotspotSupported() const {
        return platformImpl->isHotspotSupported();
    }

private:
    std::unique_ptr<WifiImpl> platformImpl;
};

// Public interface implementation
WifiManager::WifiManager() : pimpl(std::make_unique<Impl>()) {}
WifiManager::~WifiManager() = default;

std::vector<NetworkInfo> WifiManager::scan() {
    return pimpl->scan();
}

bool WifiManager::connect(const std::string& ssid, const std::string& password) {
    return pimpl->connect(ssid, password);
}

bool WifiManager::disconnect() {
    return pimpl->disconnect();
}

ConnectionStatus WifiManager::getStatus() const {
    return pimpl->getStatus();
}

bool WifiManager::createHotspot(const std::string& ssid, const std::string& password) {
    return pimpl->createHotspot(ssid, password);
}

bool WifiManager::stopHotspot() {
    return pimpl->stopHotspot();
}

bool WifiManager::isHotspotActive() const {
    return pimpl->isHotspotActive();
}

bool WifiManager::isHotspotSupported() const {
    return pimpl->isHotspotSupported();
}

} // namespace wificpp
