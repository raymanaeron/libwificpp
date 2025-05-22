#pragma once

// Platform detection macros
#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__WINDOWS__)
    #define WIFICPP_PLATFORM_WINDOWS
#elif defined(__linux__) || defined(__gnu_linux__)
    #define WIFICPP_PLATFORM_LINUX
#else
    #error "Unsupported platform"
#endif

// Define platform-specific utilities
namespace wificpp {
namespace platform {

// Determine if the platform supports WiFi operations
inline bool isSupported() {
#if defined(WIFICPP_PLATFORM_WINDOWS)
    return true;
#elif defined(WIFICPP_PLATFORM_LINUX)
    return true;
#else
    return false;
#endif
}

// Returns a string identifying the current platform
inline const char* getPlatformName() {
#ifdef WIFICPP_PLATFORM_WINDOWS
    return "Windows";
#elif defined(WIFICPP_PLATFORM_LINUX)
    return "Linux";
#else
    return "Unknown";
#endif
}

// Returns whether the platform requires elevated privileges for network operations
inline bool requiresElevatedPrivileges() {
#ifdef WIFICPP_PLATFORM_WINDOWS
    return true;  // Windows typically requires admin rights for many WiFi operations
#elif defined(WIFICPP_PLATFORM_LINUX)
    return true;  // Linux also typically requires root for managing network interfaces
#else
    return false;
#endif
}

} // namespace platform
} // namespace wificpp