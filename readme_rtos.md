# RTOS Platform Setup for libwificpp

## Supported RTOS Platforms

libwificpp provides WiFi management capabilities for the following RTOS platforms:

1. **ESP-IDF (ESP32)**
   - Full support for WiFi scanning, connection, and hotspot functionality
   - Tested with ESP-IDF v4.4+

2. **Zephyr RTOS**
   - Requires Zephyr 2.6.0 or newer
   - Support for scanning and connection

3. **FreeRTOS**
   - Generic WiFi support when using FreeRTOS with WiFi-capable hardware
   - Specific optimizations for common hardware platforms

4. **ThreadX**
   - Basic support for WiFi operations

## Hardware-Specific Configurations

The library includes optimizations for the following hardware platforms:

- ESP32, ESP32-S2, ESP32-C3
- STM32WB, STM32WL series
- Nordic nRF52 series
- TI CC32xx series

## Prerequisites

1. **ESP-IDF**
   - ESP-IDF v4.4 or newer
   - Required components: `esp_wifi`, `esp_netif`

2. **Zephyr**
   - Zephyr RTOS 2.6.0 or newer
   - Required modules: `CONFIG_WIFI=y`, `CONFIG_NET_MGMT=y`, `CONFIG_NET_L2_WIFI_MGMT=y`

3. **FreeRTOS with Other Hardware**
   - Hardware-specific WiFi drivers
   - TCP/IP stack (LwIP recommended)

## Integration Steps

### ESP-IDF (ESP32)

1. **Add library to your project**
   - For component-based projects:
     ```
     cd your_project/components
     git clone https://github.com/raymanaeron/libwificpp.git
     ```

2. **Update CMakeLists.txt**
   ```cmake
   set(COMPONENT_REQUIRES libwificpp esp_wifi esp_netif)
   ```

3. **Update sdkconfig**
   ```
   CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM=16
   CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM=32
   CONFIG_ESP32_WIFI_DYNAMIC_TX_BUFFER_NUM=32
   ```

### Zephyr RTOS

1. **Add library to your project**
   - Add as a module in your Zephyr application
     ```
     west update
     ```

2. **Update prj.conf**
   ```
   CONFIG_WIFI=y
   CONFIG_NET_MGMT=y
   CONFIG_NET_L2_WIFI_MGMT=y
   CONFIG_LIBWIFICPP=y
   ```

### FreeRTOS (Generic)

1. **Add library to your project**
   - Include source files in your build system
   - Configure library for your hardware

2. **Required configuration macros**
   ```c
   #define WIFICPP_PLATFORM_RTOS
   #define WIFICPP_FREERTOS
   
   // Define one of the following based on your hardware:
   #define WIFICPP_ESP32
   #define WIFICPP_STM32
   #define WIFICPP_NRF52
   ```

## Sample Usage

```cpp
#include "wifi_manager.hpp"
#include "wifi_logger.hpp"

extern "C" void app_main(void)
{
    // Initialize the WiFi manager
    wificpp::WifiManager wifiManager;
    
    // Set log level
    wificpp::Logger::getInstance().setLogLevel(wificpp::LogLevel::INFO);
    
    // Scan for networks
    auto networks = wifiManager.scan();
    
    // Print discovered networks
    for (const auto& network : networks) {
        printf("Network: %s, Signal: %d dBm\n", 
               network.ssid.c_str(), network.signalStrength);
    }
    
    // Connect to a network
    bool connected = wifiManager.connect("YourSSID", "YourPassword");
    if (connected) {
        printf("Successfully connected to WiFi network\n");
    }
}
```

## RTOS-Specific Considerations

### Memory Management

RTOS environments typically have limited memory. The library is designed to use minimal memory:

- No dynamic memory allocations in critical paths
- Optional static buffer pools for network scan results
- Configurable maximum network list size

Configure memory usage with the following macros:

```cpp
#define WIFICPP_MAX_NETWORKS 20        // Maximum number of networks in scan results
#define WIFICPP_USE_STATIC_BUFFERS 1   // Use static buffer allocation
```

### Task Priority and Stack Size

When creating tasks that use the WiFi library, ensure adequate stack size:

- Minimum recommended stack: 4KB for ESP32, 2KB for other platforms
- Recommended task priority: Above normal priority but below critical system tasks

Example for FreeRTOS:

```c
#define WIFI_TASK_STACK_SIZE (4096)
#define WIFI_TASK_PRIORITY (tskIDLE_PRIORITY + 2)

void wifi_task(void *pvParameters) {
    // WiFi operations
}

// Create the task
xTaskCreate(wifi_task, "WiFi Task", WIFI_TASK_STACK_SIZE, NULL, WIFI_TASK_PRIORITY, NULL);
```

### Power Management

The library supports RTOS power management:

- WiFi scanning and connection operations will prevent sleep modes
- API to enable/disable power saving modes

```cpp
// Enable WiFi power saving
wifiManager.setPowerSaveMode(wificpp::PowerSaveMode::MEDIUM);
```

## Troubleshooting

1. **Build Errors**
   - Ensure your RTOS platform is properly detected in `wifi_platform.hpp`
   - Check that all required WiFi components are included in your build

2. **Runtime Errors**
   - Enable verbose logging: `Logger::getInstance().setLogLevel(LogLevel::DEBUG);`
   - For ESP32, monitor logs with `idf.py monitor`

3. **Memory Issues**
   - Increase task stack size
   - Use static buffers with `WIFICPP_USE_STATIC_BUFFERS`
   - Reduce `WIFICPP_MAX_NETWORKS` if scanning uses too much memory

4. **WiFi Connection Failures**
   - Check signal strength before connecting
   - Verify credentials are correct
   - Ensure WiFi driver is properly initialized
   - Some hardware may require specific initialization sequences

## Known Limitations

1. **Zephyr RTOS**
   - Hotspot/AP mode is not supported on all hardware
   - Limited WPA3 support, depends on hardware

2. **ThreadX**
   - Basic implementation, may require additional hardware-specific code

3. **Power Management**
   - Deep sleep modes may disconnect WiFi on some hardware
   - Reconnection after sleep requires careful management
