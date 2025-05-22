#include "wifi_types.hpp"
#include "wifi_logger.hpp"
#include <windows.h>
#include <wlanapi.h>
#include <objbase.h>
#include <wtypes.h>

namespace wificpp {

std::string NetworkInfo::getSecurityString() const {
    switch (security) {
        case SecurityType::NONE:   return "None";
        case SecurityType::WEP:    return "WEP";
        case SecurityType::WPA:    return "WPA";
        case SecurityType::WPA2:   return "WPA2";
        case SecurityType::WPA3:   return "WPA3";
        default:                   return "Unknown";
    }
}

}
