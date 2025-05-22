# WiFi Manager Library (C++) with Rust Bindings

This project provides a WiFi management library written in C++ with Rust bindings. It allows scanning for available WiFi networks and connecting to them.

## Features

- Scan for available WiFi networks and retrieve detailed information
- Connect to WiFi networks (secured and unsecured)
- Disconnect from current network
- Get connection status
- Create and manage WiFi hotspots (unsecured/open networks)
- Check for hotspot support on device
- Cross-platform support (currently implemented for Windows using WLAN API)
- Rust bindings for easy integration with Rust applications

## Project Structure

```
libwificpp/
├── CMakeLists.txt                 # CMake build configuration
├── include/                       # C++ public headers
│   ├── wifi_manager.hpp           # Main WiFi management interface
│   ├── wifi_types.hpp             # Data structures and enums
│   ├── wifi_logger.hpp            # Logging utilities
│   └── wifi_c_api.h               # C API for FFI
├── src/                           # C++ implementation files
│   ├── wifi_manager.cpp
│   ├── wifi_scanner.cpp
│   ├── wifi_logger.cpp
│   └── wifi_c_api.cpp
├── test/                          # Test applications
│   └── test_wifi.cpp              # C++ test application
└── wifi-rs/                       # Rust bindings
    ├── Cargo.toml                 # Rust project configuration
    ├── build.rs                   # Rust build script to link C++ lib
    └── src/
        ├── lib.rs                 # Rust FFI bindings
        └── main.rs                # Rust example application
```

## Prerequisites

- Windows operating system
- CMake 3.15 or higher
- MinGW-w64 (for building the C++ code)
- Rust and Cargo (for building the Rust bindings)
- Visual Studio Code (recommended for development)

## Building

### Using VSCode Tasks

The project includes VSCode tasks for easy building:

1. Open Command Palette (Ctrl+Shift+P)
2. Type "Tasks: Run Task" and select it
3. Choose "Build All" to build both C++ and Rust projects

### Manual Building

#### C++ Library

```powershell
# From the project root
mkdir -Force build
cd build
cmake -G "MinGW Makefiles" ..
mingw32-make
```

#### Rust Bindings

```powershell
# From the project root
cd wifi-rs
cargo build
```

## Testing

### C++ Test Application

```powershell
# From the build directory
./test_wifi
```

### Rust Example Application

```powershell
# From the wifi-rs directory
cargo run
```

## Usage Examples

### C++ Example

```cpp
#include <wifi_manager.hpp>
#include <iostream>

int main() {
    wificpp::WifiManager wifi;
    
    // Scan for networks
    auto networks = wifi.scan();
    for (const auto& network : networks) {
        std::cout << "SSID: " << network.ssid 
                 << ", Signal: " << network.signalStrength << "%" << std::endl;
    }
    
    // Connect to an open network
    wifi.connect("OpenNetwork");
    
    // Connect to a secured network
    wifi.connect("SecuredNetwork", "password");
    
    // Disconnect
    wifi.disconnect();
    
    // Check if hotspot functionality is supported
    if (wifi.isHotspotSupported()) {
        // Create a WiFi hotspot with SSID "MyHotspot"
        wifi.createHotspot("MyHotspot");
        
        // Check if hotspot is active
        bool isActive = wifi.isHotspotActive();
        
        // Stop the hotspot
        wifi.stopHotspot();
    }
    
    return 0;
}
```

### Rust Example

```rust
use wifi_rs::{WiFi, SecurityType};

fn main() {
    let wifi = WiFi::new();
    
    // Scan for networks
    let networks = wifi.scan();
    for network in &networks {
        println!("SSID: {}, Security: {:?}", network.ssid, network.security_type);
    }
    
    // Connect to an open network
    wifi.connect("OpenNetwork", None);
    
    // Connect to a secured network
    wifi.connect("SecuredNetwork", Some("password"));
    
    // Disconnect
    wifi.disconnect();
    
    // Check if hotspot functionality is supported
    if wifi.is_hotspot_supported() {
        // Create a WiFi hotspot with SSID "RustHotspot"
        wifi.create_hotspot("RustHotspot");
        
        // Check if hotspot is active
        let is_active = wifi.is_hotspot_active();
        
        // Stop the hotspot
        wifi.stop_hotspot();
    }
}
```

## Using the Hotspot Functionality

The WiFi library allows creating and managing WiFi hotspots (access points) on supported hardware. This feature can be used to share internet connections or create local networks for device-to-device communication.

### Important Notes:

1. **Administrative Privileges**: Creating a WiFi hotspot typically requires administrator privileges.
2. **Hardware Support**: Not all WiFi adapters support hotspot functionality.
3. **Security**: The current implementation creates unsecured (open) hotspots. Future versions will add support for secured hotspots with password protection.

### C++ Example:

```cpp
// Check if the hardware supports hotspot functionality
if (wifi.isHotspotSupported()) {
    // Create an unsecured WiFi hotspot with SSID "MyHotspot"
    if (wifi.createHotspot("MyHotspot")) {
        std::cout << "Hotspot created successfully!" << std::endl;
        
        // Do something while the hotspot is active
        
        // Stop the hotspot when done
        wifi.stopHotspot();
    }
}
```

### Rust Example:

```rust
// Check if the hardware supports hotspot functionality
if wifi.is_hotspot_supported() {
    // Create an unsecured WiFi hotspot
    if wifi.create_hotspot("RustHotspot") {
        println!("Hotspot created successfully!");
        
        // Do something while the hotspot is active
        
        // Stop the hotspot when done
        wifi.stop_hotspot();
    }
}
```

## License

MIT License
