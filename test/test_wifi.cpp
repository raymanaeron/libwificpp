#include <iostream>
#include <string>
#include "wifi_manager.hpp"

int main() {
    try {
        // Create the WiFi manager
        wificpp::WifiManager wifi;
        
        // Scan for networks
        std::cout << "Scanning for WiFi networks...\n";
        auto networks = wifi.scan();
        std::cout << "Found " << networks.size() << " networks\n";
        
        // Print network details
        for (const auto& network : networks) {
            std::cout << "SSID: " << network.ssid
                     << ", BSSID: " << network.bssid
                     << ", Signal: " << network.signalStrength
                     << "%, Security: " << network.getSecurityString()
                     << ", Channel: " << network.channel
                     << ", Frequency: " << network.frequency << " MHz\n";
        }
        
        // Get current connection status
        auto status = wifi.getStatus();
        std::cout << "Current connection status: ";
        switch (status) {
            case wificpp::ConnectionStatus::CONNECTED:
                std::cout << "Connected\n";
                break;
            case wificpp::ConnectionStatus::DISCONNECTED:
                std::cout << "Disconnected\n";
                break;            
            case wificpp::ConnectionStatus::CONNECTING:
                std::cout << "Connecting\n";
                break;
            case wificpp::ConnectionStatus::CONNECTION_ERROR:
                std::cout << "Error\n";
                break;
        }
        
        // Check if hotspot functionality is supported
        std::cout << "\nChecking hotspot support...\n";
        if (wifi.isHotspotSupported()) {
            std::cout << "Hotspot functionality is supported on this device.\n";
            
            // Check if hotspot is already active
            if (wifi.isHotspotActive()) {
                std::cout << "A hotspot is currently active.\n";
                std::cout << "Stopping active hotspot...\n";
                if (wifi.stopHotspot()) {
                    std::cout << "Hotspot stopped successfully.\n";
                } else {
                    std::cout << "Failed to stop hotspot.\n";
                }
            }
            
            // Create a test hotspot
            std::string hotspotSsid = "TestHotspot";
            std::cout << "Creating a test hotspot with SSID: " << hotspotSsid << "...\n";
            std::cout << "Note: This requires administrative privileges.\n";
            if (wifi.createHotspot(hotspotSsid)) {
                std::cout << "Hotspot created successfully.\n";
                std::cout << "Hotspot active: " << (wifi.isHotspotActive() ? "Yes" : "No") << "\n";
                
                std::cout << "Press Enter to stop the hotspot...";
                std::cin.get();
                
                std::cout << "Stopping hotspot...\n";
                if (wifi.stopHotspot()) {
                    std::cout << "Hotspot stopped successfully.\n";
                } else {
                    std::cout << "Failed to stop hotspot.\n";
                }
            } else {
                std::cout << "Failed to create hotspot. Make sure you're running with admin privileges.\n";
            }
        } else {
            std::cout << "Hotspot functionality is not supported on this device.\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
