#[repr(C)]
pub struct RawNetworkInfo {
    ssid: *const libc::c_char,
    bssid: *const libc::c_char,
    signal_strength: libc::c_int,
    security_type: libc::c_int,
    channel: libc::c_int,
    frequency: libc::c_int,
}

// Safe wrapper for NetworkInfo
#[derive(Debug, Clone)]
pub struct NetworkInfo {
    pub ssid: String,
    pub bssid: String,
    pub signal_strength: i32,
    pub security_type: SecurityType,
    pub channel: i32,
    pub frequency: i32,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum SecurityType {
    None,
    Wep,
    Wpa,
    Wpa2,
    Wpa3,
    Unknown,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ConnectionStatus {
    Connected,
    Disconnected,
    Connecting,
    Error,
}

#[repr(C)]
pub struct WifiManager {
    _private: [u8; 0],
}

extern "C" {
    fn wifi_manager_new() -> *mut WifiManager;
    fn wifi_manager_delete(manager: *mut WifiManager);
    fn wifi_manager_scan(manager: *mut WifiManager, count: *mut libc::c_int) -> *mut RawNetworkInfo;
    fn wifi_manager_connect(manager: *mut WifiManager, ssid: *const libc::c_char, password: *const libc::c_char) -> bool;
    fn wifi_manager_disconnect(manager: *mut WifiManager) -> bool;
    fn wifi_manager_get_status(manager: *mut WifiManager) -> libc::c_int;
    fn wifi_free_network_info(networks: *mut RawNetworkInfo, count: libc::c_int);
    
    // Hotspot functions
    fn wifi_manager_create_hotspot(manager: *mut WifiManager, ssid: *const libc::c_char) -> bool;
    fn wifi_manager_stop_hotspot(manager: *mut WifiManager) -> bool;
    fn wifi_manager_is_hotspot_active(manager: *mut WifiManager) -> bool;
    fn wifi_manager_is_hotspot_supported(manager: *mut WifiManager) -> bool;
}

pub struct WiFi {
    manager: *mut WifiManager,
}

impl WiFi {
    pub fn new() -> Self {
        unsafe {
            WiFi {
                manager: wifi_manager_new(),
            }
        }
    }

    pub fn scan(&self) -> Vec<NetworkInfo> {
        unsafe {
            let mut count: libc::c_int = 0;
            let raw_networks = wifi_manager_scan(self.manager, &mut count);
            
            if raw_networks.is_null() || count <= 0 {
                return Vec::new();
            }
            
            let raw_slice = std::slice::from_raw_parts(raw_networks, count as usize);
            let result = raw_slice
                .iter()
                .map(|raw| {
                    let ssid = if raw.ssid.is_null() {
                        String::new()
                    } else {
                        std::ffi::CStr::from_ptr(raw.ssid)
                            .to_string_lossy()
                            .into_owned()
                    };
                    
                    let bssid = if raw.bssid.is_null() {
                        String::new()
                    } else {
                        std::ffi::CStr::from_ptr(raw.bssid)
                            .to_string_lossy()
                            .into_owned()
                    };
                    
                    NetworkInfo {
                        ssid,
                        bssid,
                        signal_strength: raw.signal_strength,
                        security_type: match raw.security_type {
                            0 => SecurityType::None,
                            1 => SecurityType::Wep,
                            2 => SecurityType::Wpa,
                            3 => SecurityType::Wpa2,
                            4 => SecurityType::Wpa3,
                            _ => SecurityType::Unknown,
                        },
                        channel: raw.channel,
                        frequency: raw.frequency,
                    }
                })
                .collect();
            
            wifi_free_network_info(raw_networks, count);
            result
        }
    }

    pub fn connect(&self, ssid: &str, password: Option<&str>) -> bool {
        unsafe {
            let ssid = std::ffi::CString::new(ssid).unwrap();
            let password = password.map(|p| std::ffi::CString::new(p).unwrap());
            
            wifi_manager_connect(
                self.manager,
                ssid.as_ptr(),
                password.map_or(std::ptr::null(), |p| p.as_ptr())
            )
        }
    }

    pub fn disconnect(&self) -> bool {
        unsafe {
            wifi_manager_disconnect(self.manager)
        }
    }    pub fn get_status(&self) -> ConnectionStatus {
        unsafe {            match wifi_manager_get_status(self.manager) {
                0 => ConnectionStatus::Connected,
                1 => ConnectionStatus::Disconnected,
                2 => ConnectionStatus::Connecting,
                _ => ConnectionStatus::Error,
            }
        }
    }
    
    /// Check if the hardware supports hotspot functionality.
    ///
    /// # Returns
    ///
    /// `true` if the hardware supports creating hotspots, `false` otherwise.
    pub fn is_hotspot_supported(&self) -> bool {
        unsafe {
            wifi_manager_is_hotspot_supported(self.manager)
        }
    }
    
    /// Check if a hotspot is currently active.
    ///
    /// # Returns
    ///
    /// `true` if a hotspot is active, `false` otherwise.
    pub fn is_hotspot_active(&self) -> bool {
        unsafe {
            wifi_manager_is_hotspot_active(self.manager)
        }
    }
    
    /// Create an unsecured WiFi hotspot with the given SSID.
    ///
    /// # Arguments
    ///
    /// * `ssid` - The SSID (network name) for the hotspot
    ///
    /// # Returns
    ///
    /// `true` if the hotspot was created successfully, `false` otherwise.
    ///
    /// # Note
    ///
    /// This operation typically requires administrative privileges.
    pub fn create_hotspot(&self, ssid: &str) -> bool {
        unsafe {
            let ssid = std::ffi::CString::new(ssid).unwrap();
            wifi_manager_create_hotspot(self.manager, ssid.as_ptr())
        }
    }
    
    /// Stop the active hotspot.
    ///
    /// # Returns
    ///
    /// `true` if the hotspot was stopped successfully or if no hotspot was active, `false` otherwise.
    pub fn stop_hotspot(&self) -> bool {
        unsafe {
            wifi_manager_stop_hotspot(self.manager)
        }
    }
}

impl Drop for WiFi {
    fn drop(&mut self) {
        unsafe {
            wifi_manager_delete(self.manager);
        }
    }
}

impl Default for WiFi {
    fn default() -> Self {
        Self::new()
    }
}
