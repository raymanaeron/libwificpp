#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque pointer to WifiManager
typedef struct WifiManager WifiManager;

// Network information struct for C API
typedef struct {
    const char* ssid;
    const char* bssid;
    int32_t signal_strength;
    int32_t security_type;
    int32_t channel;
    int32_t frequency;
} WifiNetworkInfo;

// Connection status enum for C API
typedef enum {
    WIFI_STATUS_CONNECTED = 0,
    WIFI_STATUS_DISCONNECTED = 1,
    WIFI_STATUS_CONNECTING = 2,
    WIFI_STATUS_ERROR = 3
} WifiConnectionStatus;

// Create a new WifiManager instance
WifiManager* wifi_manager_new();

// Delete a WifiManager instance
void wifi_manager_delete(WifiManager* manager);

// Scan for available networks
// Returns an array of WifiNetworkInfo, with the length stored in count
// The caller must free the returned array using wifi_free_network_info
WifiNetworkInfo* wifi_manager_scan(WifiManager* manager, int* count);

// Connect to a network
// If password is NULL, it will attempt to connect to an open network
// Returns true if the connection was initiated successfully
bool wifi_manager_connect(WifiManager* manager, const char* ssid, const char* password);

// Disconnect from the current network
// Returns true if the disconnection was successful
bool wifi_manager_disconnect(WifiManager* manager);

// Get the current connection status
WifiConnectionStatus wifi_manager_get_status(WifiManager* manager);

/**
 * Create an unsecured WiFi hotspot with the given SSID.
 * 
 * @param manager The WifiManager instance
 * @param ssid The SSID (network name) for the hotspot
 * @return true if the hotspot was created successfully, false otherwise
 * @note This operation typically requires administrative privileges
 */
bool wifi_manager_create_hotspot(WifiManager* manager, const char* ssid);

/**
 * Stop the active hotspot.
 * 
 * @param manager The WifiManager instance
 * @return true if the hotspot was stopped successfully or if no hotspot was active, false otherwise
 */
bool wifi_manager_stop_hotspot(WifiManager* manager);

/**
 * Check if a hotspot is currently active.
 * 
 * @param manager The WifiManager instance
 * @return true if a hotspot is active, false otherwise
 */
bool wifi_manager_is_hotspot_active(WifiManager* manager);

/**
 * Check if the hardware supports hotspot functionality.
 * 
 * @param manager The WifiManager instance
 * @return true if the hardware supports creating hotspots, false otherwise
 */
bool wifi_manager_is_hotspot_supported(WifiManager* manager);

// Free the network info array returned by wifi_manager_scan
void wifi_free_network_info(WifiNetworkInfo* networks, int count);

#ifdef __cplusplus
}
#endif
