#pragma once

// Platform detection macros
#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__WINDOWS__)
    #define WIFICPP_PLATFORM_WINDOWS
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
        #define WIFICPP_PLATFORM_IOS
    #else
        #define WIFICPP_PLATFORM_MACOS
    #endif
#elif defined(__ANDROID__)
    #define WIFICPP_PLATFORM_ANDROID
#elif defined(__linux__) || defined(__gnu_linux__)
    #define WIFICPP_PLATFORM_LINUX
#elif defined(ESP_PLATFORM) || defined(CONFIG_IDF_TARGET_ESP32)
    #define WIFICPP_PLATFORM_RTOS
    #define WIFICPP_ESP32
#elif defined(CONFIG_ZEPHYR_KERNEL)
    #define WIFICPP_PLATFORM_RTOS
    #define WIFICPP_ZEPHYR
#elif defined(TX_INCLUDE_USER_DEFINE_FILE) || defined(THREADX_MAJOR_VERSION)
    #define WIFICPP_PLATFORM_RTOS
    #define WIFICPP_THREADX
#elif defined(FREERTOS_CONFIG_H) || defined(INC_FREERTOS_H)
    #define WIFICPP_PLATFORM_RTOS
    #define WIFICPP_FREERTOS
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
#elif defined(WIFICPP_PLATFORM_MACOS)
    return true;
#elif defined(WIFICPP_PLATFORM_ANDROID)
    return true;
#elif defined(WIFICPP_PLATFORM_LINUX)
    return true;
#elif defined(WIFICPP_PLATFORM_RTOS)
    return true;
#else
    return false;
#endif
}

// Returns a string identifying the current platform
inline const char* getPlatformName() {
#ifdef WIFICPP_PLATFORM_WINDOWS
    return "Windows";
#elif defined(WIFICPP_PLATFORM_MACOS)
    return "macOS";
#elif defined(WIFICPP_PLATFORM_ANDROID)
    return "Android";
#elif defined(WIFICPP_PLATFORM_LINUX)
    return "Linux";
#elif defined(WIFICPP_PLATFORM_RTOS)
    return "RTOS";
#else
    return "Unknown";
#endif
}

// Returns whether the platform requires elevated privileges for network operations
inline bool requiresElevatedPrivileges() {
#ifdef WIFICPP_PLATFORM_WINDOWS
    return true;  // Windows typically requires admin rights for many WiFi operations
#elif defined(WIFICPP_PLATFORM_MACOS)
    return true;  // macOS typically requires admin rights for managing network interfaces
#elif defined(WIFICPP_PLATFORM_ANDROID)
    return true;  // Android typically requires elevated privileges for WiFi operations
#elif defined(WIFICPP_PLATFORM_LINUX)
    return true;  // Linux also typically requires root for managing network interfaces
#elif defined(WIFICPP_PLATFORM_RTOS)
    return false; // RTOS platforms generally do not have the same concept of elevated privileges
#else
    return false;
#endif
}

} // namespace platform
} // namespace wificpp