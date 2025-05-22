# Android Platform Setup for libwificpp

## Prerequisites

1. **Supported Android Versions**
   - Android 6.0 (API level 23) or newer
   - Android 8.0+ (API level 26+) recommended for full functionality

2. **Development Environment**
   - Android Studio 4.0 or newer
   - Android NDK 20 or newer
   - CMake 3.10 or newer
   - JDK 8 or newer

## Project Integration

1. **Gradle Configuration**
   - Add to your app's `build.gradle`:
     ```gradle
     android {
         defaultConfig {
             // ... other configurations
             externalNativeBuild {
                 cmake {
                     cppFlags "-std=c++17"
                     arguments "-DANDROID_STL=c++_shared"
                 }
             }
         }
         
         externalNativeBuild {
             cmake {
                 path "src/main/cpp/CMakeLists.txt"
             }
         }
     }
     
     dependencies {
         implementation fileTree(dir: "libs", include: ["*.jar", "*.aar"])
         // ... other dependencies
     }
     ```

2. **CMake Configuration**
   - Create `src/main/cpp/CMakeLists.txt`:
     ```cmake
     cmake_minimum_required(VERSION 3.10)
     
     # Import libwificpp
     add_library(wificpp SHARED IMPORTED)
     set_target_properties(wificpp PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libwificpp.so)
     
     # Include directories
     include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
     
     # Your native library
     add_library(your_native_lib SHARED your_source_files.cpp)
     target_link_libraries(your_native_lib wificpp log)
     ```

3. **Copy libwificpp Files**
   - Copy libwificpp library to `src/main/jniLibs/<ABI>/libwificpp.so`
   - Copy header files to `src/main/cpp/include/`
   - Copy Java helper classes to `src/main/java/com/wificpp/`

## Required Android Permissions

Add the following permissions to your `AndroidManifest.xml`:

```xml
<!-- Basic WiFi permissions -->
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.CHANGE_WIFI_STATE" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.CHANGE_NETWORK_STATE" />

<!-- Location permissions (required for WiFi scanning) -->
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
<uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION" />

<!-- For Android 10+ to allow WiFi connections in the background -->
<uses-permission android:name="android.permission.ACCESS_BACKGROUND_LOCATION" />

<!-- For hotspot functionality -->
<uses-permission android:name="android.permission.TETHER_PRIVILEGE" />
```

## Initialization in your Android App

1. **Initialize in your Application class**
   ```java
   public class YourApplication extends Application {
       @Override
       public void onCreate() {
           super.onCreate();
           
           // Load native library
           System.loadLibrary("wificpp");
           System.loadLibrary("your_native_lib");
           
           // Set application context for WifiHelper
           com.wificpp.WifiHelper.setAppContext(getApplicationContext());
       }
   }
   ```

2. **Runtime Permission Requests**
   - Location permissions must be requested at runtime
   - Example request code:
     ```java
     if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
             != PackageManager.PERMISSION_GRANTED) {
         ActivityCompat.requestPermissions(this,
                 new String[]{Manifest.permission.ACCESS_FINE_LOCATION},
                 LOCATION_PERMISSION_REQUEST_CODE);
     }
     ```

## Android API Version Considerations

1. **Android 6.0-9.0 (API 23-28)**
   - Standard WiFi API available through WifiManager
   - Location permissions required for scanning

2. **Android 10 (API 29)**
   - Restricted ability to enable/disable WiFi
   - New NetworkRequest API for WiFi connections
   - Background location permission needed for background scanning

3. **Android 11+ (API 30+)**
   - Further restrictions on background location
   - WiFi suggestion API preferred over direct connections
   - Nearby devices permission may be required

## Troubleshooting

1. **WiFi Scanning Not Working**
   - Verify location permissions are granted
   - Check if location services are enabled on the device
   - Confirm WiFi is enabled

2. **Unable to Connect to Networks**
   - For Android 10+, ensure you're using the NetworkRequest API
   - Check for proper permissions
   - Verify the WiFi network is in range

3. **Hotspot Issues**
   - Hotspot creation requires system/privileged permissions on many devices
   - Some manufacturers restrict hotspot functionality
   - Use the device's tethering settings as a fallback

## Known Limitations

1. Direct control over WiFi radio is increasingly restricted in newer Android versions
2. Hotspot functionality varies by device manufacturer and may not be fully available
3. Background WiFi operations require special permissions and may be restricted
4. Performance varies widely across device manufacturers due to driver differences
