#pragma once

#include <string>
#include <vector>

namespace wificpp {

enum class SecurityType {
    NONE,
    WEP,
    WPA,
    WPA2,
    WPA3,
    UNKNOWN
};

enum class ConnectionStatus {
    CONNECTED,
    DISCONNECTED,
    CONNECTING,
    CONNECTION_ERROR // Renamed from ERROR to avoid conflict with Windows macro
};

struct NetworkInfo {
    std::string ssid;
    std::string bssid;
    int signalStrength;     // in dBm
    SecurityType security;
    int channel;
    int frequency;          // in MHz
    
    // Additional fields that might be useful
    bool isSecure() const { return security != SecurityType::NONE; }
    std::string getSecurityString() const;
};

} // namespace wificpp
