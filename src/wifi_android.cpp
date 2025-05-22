#include "wifi_impl.hpp"
#include "wifi_logger.hpp"
#include "wifi_platform.hpp"
#include "wifi_types.hpp"
#include <memory>
#include <stdexcept>
#include <jni.h>
#include <android/log.h>

#ifdef WIFICPP_PLATFORM_ANDROID

namespace wificpp {

// JNI helper functions
static JavaVM* g_jvm = nullptr;
static jobject g_wifi_helper_obj = nullptr;
static jclass g_wifi_helper_class = nullptr;

// Helper to attach/detach threads to the JVM
class JNIEnv_Wrapper {
public:
    JNIEnv_Wrapper() : env(nullptr), attached(false) {
        if (!g_jvm) {
            __android_log_print(ANDROID_LOG_ERROR, "libwificpp", "JVM not initialized");
            return;
        }
        
        jint result = g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (result == JNI_EDETACHED) {
            if (g_jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
                attached = true;
            } else {
                __android_log_print(ANDROID_LOG_ERROR, "libwificpp", "Failed to attach thread to JVM");
            }
        } else if (result != JNI_OK) {
            __android_log_print(ANDROID_LOG_ERROR, "libwificpp", "Failed to get JNI environment");
        }
    }
    
    ~JNIEnv_Wrapper() {
        if (attached && g_jvm) {
            g_jvm->DetachCurrentThread();
        }
    }
    
    JNIEnv* operator->() const { return env; }
    operator JNIEnv*() const { return env; }
    operator bool() const { return env != nullptr; }
    
private:
    JNIEnv* env;
    bool attached;
};

// Convert jstring to std::string
std::string jstringToString(JNIEnv* env, jstring jstr) {
    if (!jstr) return "";
    const char* cstr = env->GetStringUTFChars(jstr, nullptr);
    std::string result(cstr);
    env->ReleaseStringUTFChars(jstr, cstr);
    return result;
}

// Convert std::string to jstring
jstring stringToJstring(JNIEnv* env, const std::string& str) {
    return env->NewStringUTF(str.c_str());
}

class AndroidWifiImpl : public WifiImpl {
public:
    AndroidWifiImpl() {
        JNIEnv_Wrapper env;
        if (!env || !g_wifi_helper_obj || !g_wifi_helper_class) {
            throw std::runtime_error("JNI environment not properly initialized");
        }
        
        // Call initialize method
        jmethodID initMethod = env->GetMethodID(g_wifi_helper_class, "initialize", "()Z");
        if (!initMethod) {
            throw std::runtime_error("Failed to find initialize method");
        }
        
        jboolean result = env->CallBooleanMethod(g_wifi_helper_obj, initMethod);
        if (!result) {
            throw std::runtime_error("Failed to initialize WiFi on Android");
        }
        
        Logger::getInstance().info("WifiManager initialized on Android platform");
    }
    
    ~AndroidWifiImpl() {
        // Clean up resources
    }
    
    std::vector<NetworkInfo> scan() override {
        std::vector<NetworkInfo> networks;
        Logger::getInstance().info("Scanning for networks on Android");
        
        JNIEnv_Wrapper env;
        if (!env) return networks;
        
        // Call scan method
        jmethodID scanMethod = env->GetMethodID(g_wifi_helper_class, "startScan", "()Z");
        if (!scanMethod) {
            Logger::getInstance().error("Failed to find startScan method");
            return networks;
        }
        
        jboolean scanResult = env->CallBooleanMethod(g_wifi_helper_obj, scanMethod);
        if (!scanResult) {
            Logger::getInstance().error("Scan initiation failed");
            return networks;
        }
        
        // Wait for scan results
        jmethodID getScanResultsMethod = env->GetMethodID(g_wifi_helper_class, "getScanResults", "()[Lcom/wificpp/WifiNetwork;");
        if (!getScanResultsMethod) {
            Logger::getInstance().error("Failed to find getScanResults method");
            return networks;
        }
        
        jobjectArray scanResults = (jobjectArray)env->CallObjectMethod(g_wifi_helper_obj, getScanResultsMethod);
        if (!scanResults) {
            Logger::getInstance().error("Failed to get scan results");
            return networks;
        }
        
        // Process scan results
        jsize length = env->GetArrayLength(scanResults);
        jclass networkClass = env->FindClass("com/wificpp/WifiNetwork");
        if (!networkClass) {
            Logger::getInstance().error("Failed to find WifiNetwork class");
            return networks;
        }
        
        jfieldID ssidField = env->GetFieldID(networkClass, "ssid", "Ljava/lang/String;");
        jfieldID bssidField = env->GetFieldID(networkClass, "bssid", "Ljava/lang/String;");
        jfieldID signalField = env->GetFieldID(networkClass, "signalStrength", "I");
        jfieldID frequencyField = env->GetFieldID(networkClass, "frequency", "I");
        jfieldID securityField = env->GetFieldID(networkClass, "security", "I");
        
        for (jsize i = 0; i < length; i++) {
            jobject networkObj = env->GetObjectArrayElement(scanResults, i);
            
            NetworkInfo info;
            
            jstring jssid = (jstring)env->GetObjectField(networkObj, ssidField);
            info.ssid = jstringToString(env, jssid);
            
            jstring jbssid = (jstring)env->GetObjectField(networkObj, bssidField);
            info.bssid = jstringToString(env, jbssid);
            
            info.signalStrength = env->GetIntField(networkObj, signalField);
            info.frequency = env->GetIntField(networkObj, frequencyField);
            
            // Calculate channel from frequency
            if (info.frequency >= 2412 && info.frequency <= 2484) {
                info.channel = (info.frequency - 2412) / 5 + 1;
            } else if (info.frequency >= 5170 && info.frequency <= 5825) {
                info.channel = (info.frequency - 5170) / 5 + 34;
            } else {
                info.channel = 0;
            }
            
            // Map security type
            int securityValue = env->GetIntField(networkObj, securityField);
            switch (securityValue) {
                case 0: info.security = SecurityType::NONE; break;
                case 1: info.security = SecurityType::WEP; break;
                case 2: info.security = SecurityType::WPA; break;
                case 3: info.security = SecurityType::WPA2; break;
                default: info.security = SecurityType::UNKNOWN;
            }
            
            networks.push_back(info);
            env->DeleteLocalRef(networkObj);
        }
        
        Logger::getInstance().info("Found " + std::to_string(networks.size()) + " networks");
        return networks;
    }
    
    bool connect(const std::string& ssid, const std::string& password) override {
        Logger::getInstance().info("Connecting to network: " + ssid);
        
        JNIEnv_Wrapper env;
        if (!env) return false;
        
        jmethodID connectMethod = env->GetMethodID(g_wifi_helper_class, "connect", "(Ljava/lang/String;Ljava/lang/String;)Z");
        if (!connectMethod) {
            Logger::getInstance().error("Failed to find connect method");
            return false;
        }
        
        jstring jssid = stringToJstring(env, ssid);
        jstring jpassword = stringToJstring(env, password);
        
        jboolean result = env->CallBooleanMethod(g_wifi_helper_obj, connectMethod, jssid, jpassword);
        
        env->DeleteLocalRef(jssid);
        env->DeleteLocalRef(jpassword);
        
        return result;
    }
    
    bool disconnect() override {
        Logger::getInstance().info("Disconnecting from network");
        
        JNIEnv_Wrapper env;
        if (!env) return false;
        
        jmethodID disconnectMethod = env->GetMethodID(g_wifi_helper_class, "disconnect", "()Z");
        if (!disconnectMethod) {
            Logger::getInstance().error("Failed to find disconnect method");
            return false;
        }
        
        return env->CallBooleanMethod(g_wifi_helper_obj, disconnectMethod);
    }
    
    ConnectionStatus getStatus() const override {
        JNIEnv_Wrapper env;
        if (!env) return ConnectionStatus::CONNECTION_ERROR;
        
        jmethodID getStatusMethod = env->GetMethodID(g_wifi_helper_class, "getConnectionStatus", "()I");
        if (!getStatusMethod) {
            Logger::getInstance().error("Failed to find getConnectionStatus method");
            return ConnectionStatus::CONNECTION_ERROR;
        }
        
        jint status = env->CallIntMethod(g_wifi_helper_obj, getStatusMethod);
        
        switch (status) {
            case 0: return ConnectionStatus::DISCONNECTED;
            case 1: return ConnectionStatus::CONNECTING;
            case 2: return ConnectionStatus::CONNECTED;
            default: return ConnectionStatus::CONNECTION_ERROR;
        }
    }
    
    bool createHotspot(const std::string& ssid, const std::string& password = "") override {
        Logger::getInstance().info("Creating hotspot: " + ssid);
        
        JNIEnv_Wrapper env;
        if (!env) return false;
        
        jmethodID createHotspotMethod = env->GetMethodID(g_wifi_helper_class, "createHotspot", "(Ljava/lang/String;Ljava/lang/String;)Z");
        if (!createHotspotMethod) {
            Logger::getInstance().error("Failed to find createHotspot method");
            return false;
        }
        
        jstring jssid = stringToJstring(env, ssid);
        jstring jpassword = stringToJstring(env, password);
        
        jboolean result = env->CallBooleanMethod(g_wifi_helper_obj, createHotspotMethod, jssid, jpassword);
        
        env->DeleteLocalRef(jssid);
        env->DeleteLocalRef(jpassword);
        
        return result;
    }
    
    bool stopHotspot() override {
        Logger::getInstance().info("Stopping hotspot");
        
        JNIEnv_Wrapper env;
        if (!env) return false;
        
        jmethodID stopHotspotMethod = env->GetMethodID(g_wifi_helper_class, "stopHotspot", "()Z");
        if (!stopHotspotMethod) {
            Logger::getInstance().error("Failed to find stopHotspot method");
            return false;
        }
        
        return env->CallBooleanMethod(g_wifi_helper_obj, stopHotspotMethod);
    }
    
    bool isHotspotActive() const override {
        JNIEnv_Wrapper env;
        if (!env) return false;
        
        jmethodID isHotspotActiveMethod = env->GetMethodID(g_wifi_helper_class, "isHotspotActive", "()Z");
        if (!isHotspotActiveMethod) {
            Logger::getInstance().error("Failed to find isHotspotActive method");
            return false;
        }
        
        return env->CallBooleanMethod(g_wifi_helper_obj, isHotspotActiveMethod);
    }
    
    bool isHotspotSupported() const override {
        JNIEnv_Wrapper env;
        if (!env) return false;
        
        jmethodID isHotspotSupportedMethod = env->GetMethodID(g_wifi_helper_class, "isHotspotSupported", "()Z");
        if (!isHotspotSupportedMethod) {
            Logger::getInstance().error("Failed to find isHotspotSupported method");
            return false;
        }
        
        return env->CallBooleanMethod(g_wifi_helper_obj, isHotspotSupportedMethod);
    }
};

// Factory function implementation for Android
std::unique_ptr<WifiImpl> createPlatformImpl() {
    return std::make_unique<AndroidWifiImpl>();
}

} // namespace wificpp

// JNI initialization function
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    wificpp::g_jvm = vm;
    
    JNIEnv* env;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    
    // Find WifiHelper class
    jclass localClass = env->FindClass("com/wificpp/WifiHelper");
    if (!localClass) {
        __android_log_print(ANDROID_LOG_ERROR, "libwificpp", "Failed to find WifiHelper class");
        return JNI_ERR;
    }
    
    // Create global reference to class
    wificpp::g_wifi_helper_class = (jclass)env->NewGlobalRef(localClass);
    env->DeleteLocalRef(localClass);
    
    // Create instance of WifiHelper
    jmethodID constructor = env->GetMethodID(wificpp::g_wifi_helper_class, "<init>", "()V");
    if (!constructor) {
        __android_log_print(ANDROID_LOG_ERROR, "libwificpp", "Failed to find WifiHelper constructor");
        return JNI_ERR;
    }
    
    jobject localObj = env->NewObject(wificpp::g_wifi_helper_class, constructor);
    if (!localObj) {
        __android_log_print(ANDROID_LOG_ERROR, "libwificpp", "Failed to create WifiHelper object");
        return JNI_ERR;
    }
    
    // Create global reference to object
    wificpp::g_wifi_helper_obj = env->NewGlobalRef(localObj);
    env->DeleteLocalRef(localObj);
    
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return;
    }
    
    // Delete global references
    if (wificpp::g_wifi_helper_obj) {
        env->DeleteGlobalRef(wificpp::g_wifi_helper_obj);
        wificpp::g_wifi_helper_obj = nullptr;
    }
    
    if (wificpp::g_wifi_helper_class) {
        env->DeleteGlobalRef(wificpp::g_wifi_helper_class);
        wificpp::g_wifi_helper_class = nullptr;
    }
    
    wificpp::g_jvm = nullptr;
}

#endif // WIFICPP_PLATFORM_ANDROID
