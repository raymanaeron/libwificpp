use wifi_rs::{WiFi, SecurityType};

fn main() {
    let wifi = WiFi::new();
    
    println!("Scanning for WiFi networks...");
    let networks = wifi.scan();
    println!("Found {} networks", networks.len());
    
    for network in &networks {
        println!(
            "SSID: {}, BSSID: {}, Signal: {}%, Security: {:?}, Channel: {}, Frequency: {} MHz",
            network.ssid,
            network.bssid,
            network.signal_strength,
            network.security_type,
            network.channel,
            network.frequency
        );
    }
    
    // Example: Connect to an open network
    if !networks.is_empty() {
        let open_networks: Vec<_> = networks.iter()
            .filter(|n| n.security_type == SecurityType::None)
            .collect();
        
        if !open_networks.is_empty() {
            let network = &open_networks[0];
            println!("Attempting to connect to open network: {}", network.ssid);
            
            if wifi.connect(&network.ssid, None) {
                println!("Connection initiated successfully");
                println!("Connection status: {:?}", wifi.get_status());
            } else {
                println!("Failed to connect to the network");
            }
        } else {            println!("No open networks available for automatic connection");
            
            // Example: How to connect to a secured network
            // Replace with actual network credentials for testing
            // if wifi.connect("YourNetworkSSID", Some("YourPassword")) {
            //     println!("Connected to secured network");
            // }
        }
    }
    
    // Check hotspot support and functionality
    println!("\nChecking hotspot functionality...");
    if wifi.is_hotspot_supported() {
        println!("Hotspot functionality is supported on this device");
        
        // Check if a hotspot is already active
        if wifi.is_hotspot_active() {
            println!("A hotspot is currently active");
            println!("Stopping active hotspot...");
            if wifi.stop_hotspot() {
                println!("Hotspot stopped successfully");
            } else {
                println!("Failed to stop hotspot");
            }
        }
        
        // Create a test hotspot (requires admin privileges)
        let hotspot_ssid = "RustHotspot";
        println!("Creating a test hotspot with SSID: {}", hotspot_ssid);
        println!("Note: This requires administrative privileges");
        
        if wifi.create_hotspot(hotspot_ssid) {
            println!("Hotspot created successfully");
            println!("Hotspot active: {}", wifi.is_hotspot_active());
            
            println!("Press Enter to stop the hotspot...");
            let mut input = String::new();
            std::io::stdin().read_line(&mut input).unwrap();
            
            println!("Stopping hotspot...");
            if wifi.stop_hotspot() {
                println!("Hotspot stopped successfully");
            } else {
                println!("Failed to stop hotspot");
            }
        } else {
            println!("Failed to create hotspot. Make sure you're running with admin privileges");
        }    } else {
        println!("Hotspot functionality is not supported on this device");
    }
}
