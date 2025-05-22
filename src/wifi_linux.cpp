#include "wifi_impl.hpp"
#include "wifi_logger.hpp"
#include "wifi_platform.hpp"

#ifdef WIFICPP_PLATFORM_LINUX

#include <memory>
#include <stdexcept>
#include <NetworkManager.h>
#include <glib.h>
#include <vector>

namespace wificpp {

class LinuxWifiImpl : public WifiImpl {
private:
    NMClient* client;
    NMDeviceWifi* wifi_device;
    GError* error;

    void initializeNM() {
        error = nullptr;
        client = nm_client_new(nullptr, &error);
        if (!client) {
            std::string err_msg = "Failed to create NetworkManager client";
            if (error) {
                err_msg += ": " + std::string(error->message);
                g_error_free(error);
            }
            throw std::runtime_error(err_msg);
        }

        // Find WiFi device
        const GPtrArray* devices = nm_client_get_devices(client);
        wifi_device = nullptr;
        
        for (guint i = 0; devices && i < devices->len; i++) {
            NMDevice* device = NM_DEVICE(g_ptr_array_index(devices, i));
            if (NM_IS_DEVICE_WIFI(device)) {
                wifi_device = NM_DEVICE_WIFI(device);
                break;
            }
        }

        if (!wifi_device) {
            g_object_unref(client);
            throw std::runtime_error("No WiFi device found");
        }
    }

public:
    LinuxWifiImpl() {
        Logger::getInstance().info("Initializing WiFi manager on Linux platform");
        initializeNM();
    }

    ~LinuxWifiImpl() {
        if (client) {
            g_object_unref(client);
        }
    }

    std::vector<NetworkInfo> scan() override {
        std::vector<NetworkInfo> networks;
        Logger::getInstance().info("Scanning for networks...");

        // Request a scan
        nm_device_wifi_request_scan(wifi_device, nullptr, nullptr);

        // Get access points
        const GPtrArray* aps = nm_device_wifi_get_access_points(wifi_device);
        
        for (guint i = 0; aps && i < aps->len; i++) {
            NMAccessPoint* ap = NM_ACCESS_POINT(g_ptr_array_index(aps, i));
            
            NetworkInfo info;
            info.ssid = std::string((const char*)nm_access_point_get_ssid(ap));
            info.signalStrength = nm_access_point_get_strength(ap);
            
            // Get security flags
            NM80211ApFlags flags = nm_access_point_get_flags(ap);
            NM80211ApSecurityFlags wpa_flags = nm_access_point_get_wpa_flags(ap);
            NM80211ApSecurityFlags rsn_flags = nm_access_point_get_rsn_flags(ap);
            
            if ((flags & NM_802_11_AP_FLAGS_PRIVACY) || 
                wpa_flags != NM_802_11_AP_SEC_NONE || 
                rsn_flags != NM_802_11_AP_SEC_NONE) {
                info.securityEnabled = true;
            }

            networks.push_back(info);
        }

        return networks;
    }

    bool connect(const std::string& ssid, const std::string& password) override {
        Logger::getInstance().info("Connecting to network: " + ssid);

        // Find the access point with the given SSID
        const GPtrArray* aps = nm_device_wifi_get_access_points(wifi_device);
        NMAccessPoint* target_ap = nullptr;
        
        for (guint i = 0; aps && i < aps->len; i++) {
            NMAccessPoint* ap = NM_ACCESS_POINT(g_ptr_array_index(aps, i));
            const GBytes* ssid_bytes = nm_access_point_get_ssid(ap);
            if (ssid_bytes) {
                gsize len;
                const guint8* data = (const guint8*)g_bytes_get_data(ssid_bytes, &len);
                std::string ap_ssid((const char*)data, len);
                if (ap_ssid == ssid) {
                    target_ap = ap;
                    break;
                }
            }
        }

        if (!target_ap) {
            Logger::getInstance().error("Network not found: " + ssid);
            return false;
        }

        // Create connection settings
        NMConnection* connection = nm_simple_connection_new();
        NMSettingWireless* s_wireless = (NMSettingWireless*)nm_setting_wireless_new();
        NMSettingWirelessSecurity* s_wsec = nullptr;
        
        g_object_set(s_wireless,
                    "ssid", g_bytes_new(ssid.c_str(), ssid.length()),
                    NULL);

        nm_connection_add_setting(connection, NM_SETTING(s_wireless));

        // Add security settings if password is provided
        if (!password.empty()) {
            s_wsec = (NMSettingWirelessSecurity*)nm_setting_wireless_security_new();
            g_object_set(s_wsec,
                        "key-mgmt", "wpa-psk",
                        "psk", password.c_str(),
                        NULL);
            nm_connection_add_setting(connection, NM_SETTING(s_wsec));
        }

        // Activate the connection
        GError* error = nullptr;
        NMActiveConnection* active_conn = nm_client_add_and_activate_connection(
            client,
            connection,
            NM_DEVICE(wifi_device),
            nm_access_point_get_path(target_ap),
            nullptr,
            &error
        );

        if (!active_conn) {
            Logger::getInstance().error("Failed to activate connection: " + 
                std::string(error ? error->message : "unknown error"));
            if (error) {
                g_error_free(error);
            }
            g_object_unref(connection);
            return false;
        }

        g_object_unref(connection);
        return true;
    }

    bool disconnect() override {
        Logger::getInstance().info("Disconnecting from network");

        NMActiveConnection* active = nm_client_get_primary_connection(client);
        if (!active) {
            return false;
        }

        return nm_client_deactivate_connection(client, active, nullptr, nullptr);
    }

    ConnectionStatus getStatus() const override {
        NMActiveConnection* active = nm_client_get_primary_connection(client);
        if (!active) {
            return ConnectionStatus::DISCONNECTED;
        }

        NMActiveConnectionState state = nm_active_connection_get_state(active);
        switch (state) {
            case NM_ACTIVE_CONNECTION_STATE_ACTIVATED:
                return ConnectionStatus::CONNECTED;
            case NM_ACTIVE_CONNECTION_STATE_ACTIVATING:
                return ConnectionStatus::CONNECTING;
            case NM_ACTIVE_CONNECTION_STATE_DEACTIVATING:
                return ConnectionStatus::DISCONNECTING;
            default:
                return ConnectionStatus::DISCONNECTED;
        }
    }

    bool createHotspot(const std::string& ssid) override {
        Logger::getInstance().info("Creating hotspot: " + ssid);
        // TODO: Implement hotspot creation using NetworkManager
        // This requires additional configuration and AP mode support
        return false;
    }

    bool stopHotspot() override {
        Logger::getInstance().info("Stopping hotspot");
        // TODO: Implement hotspot shutdown
        return false;
    }

    bool isHotspotActive() const override {
        // TODO: Implement hotspot status check
        return false;
    }

    bool isHotspotSupported() const override {
        // Check if the device supports AP mode
        if (!wifi_device) {
            return false;
        }
        return nm_device_wifi_get_capabilities(wifi_device) & NM_WIFI_DEVICE_CAP_AP;
    }
};

// Factory function implementation
std::unique_ptr<WifiImpl> createPlatformImpl() {
    return std::make_unique<LinuxWifiImpl>();
}

} // namespace wificpp

#endif // WIFICPP_PLATFORM_LINUX
