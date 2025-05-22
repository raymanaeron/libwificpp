# Windows Platform Setup for libwificpp

## Prerequisites

1. **Windows 7 or newer**
   - Windows 10/11 is recommended for full functionality
   - Both 32-bit and 64-bit Windows are supported

2. **Required Windows Components**
   - Ensure the Windows Native WiFi API is available (included by default in most Windows installations)
   - The Wired AutoConfig service must be running

3. **Development Tools**
   - Visual Studio 2017 or newer with C++ development tools
   - CMake 3.12 or newer
   - Git (optional, for source control)

## Installation Steps

1. **Install the library**
   ```bash
   git clone https://github.com/yourusername/libwificpp.git
   cd libwificpp
   mkdir build && cd build
   cmake ..
   cmake --build . --config Release
   cmake --install .
   ```

2. **Required DLLs**
   - Make sure the following DLLs are accessible to your application:
     - `wlanapi.dll` (Windows Native WiFi API)
     - `ole32.dll` (COM support)

3. **Administrative Privileges**
   - Many WiFi operations require administrator permissions
   - Either run your application as administrator or add a manifest file to request elevated privileges

## Usage in Your Project

1. **CMake Integration**
   ```cmake
   find_package(wificpp REQUIRED)
   target_link_libraries(your_project_name PRIVATE wificpp)
   ```

2. **Manual Linking**
   - Include directory: `<install_prefix>/include`
   - Library directory: `<install_prefix>/lib`
   - Link against: `wificpp.lib`
   - Add required Windows libraries: `wlanapi.lib` and `ole32.lib`

## Permissions and Security

1. **Windows Firewall**
   - Your application may trigger Windows Firewall alerts when managing WiFi connections
   - Consider adding appropriate firewall exceptions in your installer

2. **User Account Control (UAC)**
   - Add a manifest file to your application to request appropriate privileges:

   ```xml
   <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
   <assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
     <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
       <security>
         <requestedPrivileges>
           <requestedExecutionLevel level="requireAdministrator" uiAccess="false"/>
         </requestedPrivileges>
       </security>
     </trustInfo>
   </assembly>
   ```

## Troubleshooting

1. **Missing WLAN API**
   - Ensure the Wired AutoConfig service is running:
     ```
     sc query "Wlansvc"
     sc start "Wlansvc"
     ```

2. **Access Denied Errors**
   - Check that your application is running with administrative privileges
   - Verify the Windows user has permission to manage WiFi connections

3. **WiFi Adapter Not Found**
   - Ensure WiFi hardware is enabled (check hardware switches or airplane mode)
   - Verify WiFi drivers are properly installed

## Known Limitations

1. WiFi Direct and WiFi Aware capabilities require Windows 10 or newer
2. WPA3 support requires Windows 10 version 1903 or newer
3. Hotspot functionality may be limited by hardware capabilities
