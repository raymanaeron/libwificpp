#include "wifi_impl.hpp"
#include "wifi_logger.hpp"
#include "wifi_platform.hpp"
#include "wifi_types.hpp"
#include <memory>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/wireless.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <linux/nl80211.h>
#include <dirent.h>
#include <signal.h>
#include <wait.h>

namespace wificpp {

class LinuxWifiImpl : public WifiImpl {
public:
    LinuxWifiImpl() {
        // Initialize nl80211 socket
        socket = nl_socket_alloc();
        if (!socket) {
            throw std::runtime_error("Failed to allocate netlink socket");
        }

        // Connect to generic netlink
        if (genl_connect(socket) < 0) {
            nl_socket_free(socket);
            throw std::runtime_error("Failed to connect to generic netlink");
        }

        // Find nl80211 driver ID
        nl80211_id = genl_ctrl_resolve(socket, "nl80211");
        if (nl80211_id < 0) {
            nl_socket_free(socket);
            throw std::runtime_error("Failed to find nl80211 netlink family");
        }

        // Get interface information
        if (!findWifiInterface()) {
            nl_socket_free(socket);
            throw std::runtime_error("No WiFi interface found");
        }

        Logger::getInstance().info("WifiManager initialized on Linux platform with interface " + interface_name);
    }

    ~LinuxWifiImpl() {
        if (socket) {
            nl_socket_free(socket);
            socket = nullptr;
        }
    }

    std::vector<NetworkInfo> scan() override {
        std::vector<NetworkInfo> networks;
        Logger::getInstance().info("Scanning for networks on Linux interface " + interface_name);
        
        // Trigger scan
        struct nl_msg* msg = nlmsg_alloc();
        if (!msg) {
            Logger::getInstance().error("Failed to allocate netlink message");
            return networks;
        }

        // Setup scan request
        genlmsg_put(msg, 0, 0, nl80211_id, 0, 0, NL80211_CMD_TRIGGER_SCAN, 0);
        nla_put_u32(msg, NL80211_ATTR_IFINDEX, interface_index);

        // Send scan request
        int ret = nl_send_auto(socket, msg);
        nlmsg_free(msg);
        if (ret < 0) {
            Logger::getInstance().error("Failed to send scan request");
            return networks;
        }

        // Wait for scan to complete (5 seconds max)
        sleep(5);
        
        // Get scan results
        msg = nlmsg_alloc();
        if (!msg) {
            Logger::getInstance().error("Failed to allocate netlink message for scan results");
            return networks;
        }

        genlmsg_put(msg, 0, 0, nl80211_id, 0, NLM_F_DUMP, NL80211_CMD_GET_SCAN, 0);
        nla_put_u32(msg, NL80211_ATTR_IFINDEX, interface_index);

        // Define callback data
        struct callback_data {
            std::vector<NetworkInfo>* networks;
        } cb_data;
        cb_data.networks = &networks;

        // Define callback function to process scan results
        auto callback = [](struct nl_msg* msg, void* arg) -> int {
            struct callback_data* data = static_cast<callback_data*>(arg);
            struct nlattr* tb[NL80211_ATTR_MAX + 1];
            struct genlmsghdr* gnlh = static_cast<genlmsghdr*>(nlmsg_data(nlmsg_hdr(msg)));
            
            nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), nullptr);
            
            if (!tb[NL80211_ATTR_BSS]) {
                return NL_SKIP;
            }
            
            struct nlattr* bss[NL80211_BSS_MAX + 1];
            static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {};
            nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS], bss_policy);
            
            if (!bss[NL80211_BSS_BSSID] || !bss[NL80211_BSS_INFORMATION_ELEMENTS]) {
                return NL_SKIP;
            }
            
            NetworkInfo network;
            
            // Extract BSSID
            char mac_addr[18];
            uint8_t* bssid = static_cast<uint8_t*>(nla_data(bss[NL80211_BSS_BSSID]));
            snprintf(mac_addr, sizeof(mac_addr), "%02X:%02X:%02X:%02X:%02X:%02X",
                     bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
            network.bssid = mac_addr;
            
            // Extract information elements to get SSID and other data
            uint8_t* ie = static_cast<uint8_t*>(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]));
            int ie_len = nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
            
            for (int i = 0; i < ie_len; i += ie[i + 1] + 2) {
                if (ie[i] == 0) { // SSID element
                    network.ssid = std::string(reinterpret_cast<char*>(&ie[i + 2]), ie[i + 1]);
                }
            }
            
            // Extract signal strength
            if (bss[NL80211_BSS_SIGNAL_MBM]) {
                int signal_mbm = nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM]);
                network.signalStrength = signal_mbm / 100; // Convert to dBm
            }
            
            // Extract frequency
            if (bss[NL80211_BSS_FREQUENCY]) {
                network.frequency = nla_get_u32(bss[NL80211_BSS_FREQUENCY]);
                network.channel = frequencyToChannel(network.frequency);
            }
            
            // Extract security information
            if (bss[NL80211_BSS_CAPABILITY]) {
                uint16_t capability = nla_get_u16(bss[NL80211_BSS_CAPABILITY]);
                if (capability & (1 << 4)) { // Privacy bit
                    // Extract security details from IEs
                    bool wpa = false, rsn = false;
                    for (int i = 0; i < ie_len; i += ie[i + 1] + 2) {
                        if (ie[i] == 48) rsn = true; // RSN element (WPA2)
                        if (ie[i] == 221 && ie[i + 1] >= 4 && 
                            ie[i + 2] == 0x00 && ie[i + 3] == 0x50 && 
                            ie[i + 4] == 0xf2 && ie[i + 5] == 0x01) {
                            wpa = true; // WPA element
                        }
                    }
                    
                    if (rsn) network.security = SecurityType::WPA2;
                    else if (wpa) network.security = SecurityType::WPA;
                    else network.security = SecurityType::WEP;
                } else {
                    network.security = SecurityType::NONE;
                }
            } else {
                network.security = SecurityType::UNKNOWN;
            }
            
            // Add network if it's not already in the list
            if (!network.ssid.empty()) {
                bool exists = false;
                for (const auto& existing : *(data->networks)) {
                    if (existing.ssid == network.ssid) {
                        exists = true;
                        break;
                    }
                }
                
                if (!exists) {
                    data->networks->push_back(network);
                }
            }
            
            return NL_SKIP;
        };

        // Setup callback
        struct nl_cb* cb = nl_cb_alloc(NL_CB_DEFAULT);
        nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, callback, &cb_data);
        
        // Send message and receive response
        ret = nl_send_auto(socket, msg);
        nlmsg_free(msg);
        if (ret < 0) {
            Logger::getInstance().error("Failed to send scan results request");
            nl_cb_put(cb);
            return networks;
        }
        
        // Receive and process responses
        nl_recvmsgs(socket, cb);
        nl_cb_put(cb);
        
        Logger::getInstance().info("Found " + std::to_string(networks.size()) + " networks");
        return networks;
    }

    bool connect(const std::string& ssid, const std::string& password) override {
        Logger::getInstance().info("Connecting to network: " + ssid);
        
        // Generate wpa_supplicant configuration
        std::string config_path = "/tmp/wificpp_" + ssid + ".conf";
        std::ofstream config_file(config_path);
        if (!config_file) {
            Logger::getInstance().error("Failed to create temporary configuration file");
            return false;
        }
        
        config_file << "ctrl_interface=/var/run/wpa_supplicant\n";
        config_file << "network={\n";
        config_file << "    ssid=\"" << ssid << "\"\n";
        
        if (password.empty()) {
            config_file << "    key_mgmt=NONE\n";
        } else {
            config_file << "    psk=\"" << password << "\"\n";
            config_file << "    key_mgmt=WPA-PSK\n";
        }
        
        config_file << "}\n";
        config_file.close();
        
        // Stop existing wpa_supplicant using native process termination
        terminateProcess("wpa_supplicant");
        
        // Start wpa_supplicant with our configuration using fork/exec
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            execl("/sbin/wpa_supplicant", "wpa_supplicant", "-B", "-i", 
                  interface_name.c_str(), "-c", config_path.c_str(), NULL);
            exit(1); // Exit if exec fails
        } else if (pid < 0) {
            Logger::getInstance().error("Failed to fork process for wpa_supplicant");
            std::remove(config_path.c_str());
            return false;
        }
        
        // Wait for connection to initialize
        sleep(2);
        
        // Start DHCP client using native implementation
        bool dhcp_success = configureDhcpClient(interface_name);
        if (!dhcp_success) {
            Logger::getInstance().warning("DHCP configuration failed, but connection might still be established");
        }
        
        // Clean up temporary file
        std::remove(config_path.c_str());
        
        // Verify connection by checking for IP address
        return hasIpAddress(interface_name);
    }

    bool disconnect() override {
        Logger::getInstance().info("Disconnecting from network on " + interface_name);
        
        // Stop wpa_supplicant using native method
        terminateProcess("wpa_supplicant");
        
        // Release DHCP lease
        releaseDhcpLease(interface_name);
        
        // Set interface down/up using direct ioctl calls
        setInterfaceState(interface_name, false);
        sleep(1);
        setInterfaceState(interface_name, true);
        
        return true;
    }

    ConnectionStatus getStatus() const override {
        // Check if interface is up using ioctl
        if (!isInterfaceUp(interface_name)) {
            return ConnectionStatus::DISCONNECTED;
        }
        
        // Check if we have an IP address
        if (!hasIpAddress(interface_name)) {
            return ConnectionStatus::CONNECTING;
        }
        
        // Check if wpa_supplicant is running
        if (!isProcessRunning("wpa_supplicant")) {
            return ConnectionStatus::CONNECTION_ERROR;
        }
        
        return ConnectionStatus::CONNECTED;
    }

    bool createHotspot(const std::string& ssid, const std::string& password = "") override {
        Logger::getInstance().info("Creating hotspot: " + ssid);
        
        // Stop any existing hotspot or connection
        stopHotspot();
        disconnect();
        
        // Create hostapd configuration
        std::string config_path = "/tmp/hostapd_" + ssid + ".conf";
        std::ofstream config_file(config_path);
        if (!config_file) {
            Logger::getInstance().error("Failed to create hostapd configuration file");
            return false;
        }
        
        config_file << "interface=" << interface_name << "\n";
        config_file << "driver=nl80211\n";
        config_file << "ssid=" << ssid << "\n";
        config_file << "hw_mode=g\n";
        config_file << "channel=6\n";
        config_file << "ieee80211n=1\n";
        
        if (!password.empty()) {
            config_file << "wpa=2\n";
            config_file << "wpa_passphrase=" << password << "\n";
            config_file << "wpa_key_mgmt=WPA-PSK\n";
            config_file << "wpa_pairwise=TKIP CCMP\n";
            config_file << "rsn_pairwise=CCMP\n";
        }
        
        config_file.close();
        
        // Configure interface for AP mode
        std::string cmd = "ip link set " + interface_name + " down";
        system(cmd.c_str());
        sleep(1);
        
        // Set static IP for AP interface
        cmd = "ip addr flush dev " + interface_name;
        system(cmd.c_str());
        cmd = "ip addr add 192.168.4.1/24 dev " + interface_name;
        system(cmd.c_str());
        cmd = "ip link set " + interface_name + " up";
        system(cmd.c_str());
        
        // Start hostapd
        cmd = "hostapd -B " + config_path;
        int result = system(cmd.c_str());
        if (result != 0) {
            Logger::getInstance().error("Failed to start hostapd");
            std::remove(config_path.c_str());
            return false;
        }
        
        // Configure DHCP server (dnsmasq)
        std::string dnsmasq_conf = "/tmp/dnsmasq_" + ssid + ".conf";
        std::ofstream dnsmasq_file(dnsmasq_conf);
        if (!dnsmasq_file) {
            Logger::getInstance().error("Failed to create dnsmasq configuration");
            system("killall hostapd");
            std::remove(config_path.c_str());
            return false;
        }
        
        dnsmasq_file << "interface=" << interface_name << "\n";
        dnsmasq_file << "dhcp-range=192.168.4.2,192.168.4.20,255.255.255.0,24h\n";
        dnsmasq_file << "bind-interfaces\n";
        dnsmasq_file.close();
        
        // Start DHCP server
        cmd = "dnsmasq --conf-file=" + dnsmasq_conf;
        result = system(cmd.c_str());
        if (result != 0) {
            Logger::getInstance().error("Failed to start DHCP server");
            system("killall hostapd");
            std::remove(config_path.c_str());
            std::remove(dnsmasq_conf.c_str());
            return false;
        }
        
        // Enable IP forwarding and NAT if there's another active interface for internet sharing
        cmd = "sysctl -w net.ipv4.ip_forward=1";
        system(cmd.c_str());
        
        // Find default gateway interface
        FILE* pipe = popen("ip route | grep default | awk '{print $5}'", "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string ext_iface = buffer;
                // Remove newline character
                ext_iface.erase(std::remove(ext_iface.begin(), ext_iface.end(), '\n'), ext_iface.end());
                
                if (!ext_iface.empty() && ext_iface != interface_name) {
                    // Setup NAT
                    cmd = "iptables -t nat -A POSTROUTING -o " + ext_iface + " -j MASQUERADE";
                    system(cmd.c_str());
                    cmd = "iptables -A FORWARD -i " + interface_name + " -o " + ext_iface + " -j ACCEPT";
                    system(cmd.c_str());
                    cmd = "iptables -A FORWARD -i " + ext_iface + " -o " + interface_name + " -m state --state RELATED,ESTABLISHED -j ACCEPT";
                    system(cmd.c_str());
                }
            }
            pclose(pipe);
        }
        
        hostapd_conf_path = config_path;
        dnsmasq_conf_path = dnsmasq_conf;
        hotspot_active = true;
        
        return true;
    }

    bool stopHotspot() override {
        Logger::getInstance().info("Stopping hotspot");
        
        if (!hotspot_active) {
            return true;  // Nothing to do
        }
        
        // Stop hostapd and dnsmasq
        system("killall -q hostapd");
        system("killall -q dnsmasq");
        
        // Clean up configuration files
        if (!hostapd_conf_path.empty()) {
            std::remove(hostapd_conf_path.c_str());
            hostapd_conf_path.clear();
        }
        
        if (!dnsmasq_conf_path.empty()) {
            std::remove(dnsmasq_conf_path.c_str());
            dnsmasq_conf_path.clear();
        }
        
        // Remove NAT rules
        system("iptables -t nat -F POSTROUTING");
        system("iptables -F FORWARD");
        
        // Disable IP forwarding
        system("sysctl -w net.ipv4.ip_forward=0");
        
        // Reset interface
        std::string cmd = "ip addr flush dev " + interface_name;
        system(cmd.c_str());
        cmd = "ip link set " + interface_name + " down";
        system(cmd.c_str());
        sleep(1);
        cmd = "ip link set " + interface_name + " up";
        system(cmd.c_str());
        
        hotspot_active = false;
        return true;
    }

    bool isHotspotActive() const override {
        // Check if hostapd is running
        std::string cmd = "pgrep -x hostapd";
        int result = system((cmd + " > /dev/null 2>&1").c_str());
        return result == 0;
    }

    bool isHotspotSupported() const override {
        // Check if we have hostapd installed
        std::string cmd = "which hostapd";
        int result = system((cmd + " > /dev/null 2>&1").c_str());
        if (result != 0) {
            return false;
        }
        
        // Check if interface supports AP mode
        struct nl_msg* msg = nlmsg_alloc();
        if (!msg) {
            return false;
        }
        
        genlmsg_put(msg, 0, 0, nl80211_id, 0, 0, NL80211_CMD_GET_WIPHY, 0);
        nla_put_u32(msg, NL80211_ATTR_IFINDEX, interface_index);
        
        bool supported = false;
        struct nl_cb* cb = nl_cb_alloc(NL_CB_DEFAULT);
        
        // Callback to check for AP mode support
        auto callback = [](struct nl_msg* msg, void* arg) -> int {
            bool* supported = static_cast<bool*>(arg);
            struct nlattr* tb[NL80211_ATTR_MAX + 1];
            struct genlmsghdr* gnlh = static_cast<genlmsghdr*>(nlmsg_data(nlmsg_hdr(msg)));
            
            nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), nullptr);
            
            if (tb[NL80211_ATTR_SUPPORTED_IFTYPES]) {
                struct nlattr* nl_mode;
                int rem_mode;
                nla_for_each_nested(nl_mode, tb[NL80211_ATTR_SUPPORTED_IFTYPES], rem_mode) {
                    if (nla_type(nl_mode) == NL80211_IFTYPE_AP) {
                        *supported = true;
                        break;
                    }
                }
            }
            
            return NL_SKIP;
        };
        
        nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, callback, &supported);
        
        int ret = nl_send_auto(socket, msg);
        nlmsg_free(msg);
        
        if (ret >= 0) {
            nl_recvmsgs(socket, cb);
        }
        
        nl_cb_put(cb);
        return supported;
    }

private:
    struct nl_sock* socket = nullptr;
    int nl80211_id = -1;
    std::string interface_name;
    int interface_index = -1;
    std::string hostapd_conf_path;
    std::string dnsmasq_conf_path;
    bool hotspot_active = false;
    
    bool findWifiInterface() {
        // Find first wireless interface
        FILE* pipe = popen("iw dev | grep Interface | awk '{print $2}'", "r");
        if (!pipe) {
            return false;
        }
        
        char buffer[128];
        bool found = false;
        
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            // Remove newline character
            std::string iface(buffer);
            iface.erase(std::remove(iface.begin(), iface.end(), '\n'), iface.end());
            
            if (!iface.empty()) {
                interface_name = iface;
                found = true;
                break;
            }
        }
        
        pclose(pipe);
        
        if (found) {
            // Get interface index
            struct ifreq ifr;
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock < 0) {
                return false;
            }
            
            memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, interface_name.c_str(), IFNAMSIZ - 1);
            
            if (ioctl(sock, SIOCGIFINDEX, &ifr) >= 0) {
                interface_index = ifr.ifr_ifindex;
                close(sock);
                return true;
            }
            
            close(sock);
        }
        
        return false;
    }
    
    int frequencyToChannel(int frequency) {
        if (frequency >= 2412 && frequency <= 2484) {
            return (frequency - 2412) / 5 + 1;
        } else if (frequency >= 5170 && frequency <= 5825) {
            return (frequency - 5170) / 5 + 34;
        } else {
            return 0;
        }
    }
    
    // Helper methods for native implementation
    
    bool terminateProcess(const std::string& process_name) {
        DIR* dir = opendir("/proc");
        if (!dir) {
            return false;
        }
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            // Check if the entry is a PID directory
            if (entry->d_type == DT_DIR) {
                char* endptr;
                long pid = strtol(entry->d_name, &endptr, 10);
                if (*endptr == '\0') {
                    // Found a process directory, check command line
                    std::string cmd_path = "/proc/" + std::string(entry->d_name) + "/cmdline";
                    std::ifstream cmd_file(cmd_path);
                    std::string cmd_line;
                    if (cmd_file >> cmd_line) {
                        // Command line might contain null bytes, need to extract executable name
                        std::string exe_name = cmd_line.substr(0, cmd_line.find('\0'));
                        exe_name = exe_name.substr(exe_name.find_last_of('/') + 1);
                        
                        if (exe_name == process_name) {
                            // Found process, terminate it
                            if (kill(pid, SIGTERM) == 0) {
                                // Wait for process to terminate
                                for (int i = 0; i < 10; i++) {
                                    if (kill(pid, 0) != 0) {
                                        break; // Process terminated
                                    }
                                    usleep(100000); // 0.1 seconds
                                }
                                // Force kill if still running
                                if (kill(pid, 0) == 0) {
                                    kill(pid, SIGKILL);
                                }
                                closedir(dir);
                                return true;
                            }
                        }
                    }
                }
            }
        }
        
        closedir(dir);
        return false;
    }
    
    bool isProcessRunning(const std::string& process_name) const {
        DIR* dir = opendir("/proc");
        if (!dir) {
            return false;
        }
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_DIR) {
                char* endptr;
                strtol(entry->d_name, &endptr, 10);
                if (*endptr == '\0') {
                    std::string cmd_path = "/proc/" + std::string(entry->d_name) + "/cmdline";
                    std::ifstream cmd_file(cmd_path);
                    std::string cmd_line;
                    if (cmd_file >> cmd_line) {
                        std::string exe_name = cmd_line.substr(0, cmd_line.find('\0'));
                        exe_name = exe_name.substr(exe_name.find_last_of('/') + 1);
                        
                        if (exe_name == process_name) {
                            closedir(dir);
                            return true;
                        }
                    }
                }
            }
        }
        
        closedir(dir);
        return false;
    }
    
    bool setInterfaceState(const std::string& iface, bool up) const {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            return false;
        }
        
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, iface.c_str(), IFNAMSIZ - 1);
        
        // Get current flags
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
            close(sock);
            return false;
        }
        
        // Modify flags
        if (up) {
            ifr.ifr_flags |= IFF_UP;
        } else {
            ifr.ifr_flags &= ~IFF_UP;
        }
        
        // Set new flags
        if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
            close(sock);
            return false;
        }
        
        close(sock);
        return true;
    }
    
    bool isInterfaceUp(const std::string& iface) const {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            return false;
        }
        
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, iface.c_str(), IFNAMSIZ - 1);
        
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
            close(sock);
            return false;
        }
        
        close(sock);
        return (ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING);
    }
    
    bool hasIpAddress(const std::string& iface) const {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            return false;
        }
        
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, iface.c_str(), IFNAMSIZ - 1);
        ifr.ifr_addr.sa_family = AF_INET;
        
        if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
            close(sock);
            return false;
        }
        
        close(sock);
        struct sockaddr_in* sin = (struct sockaddr_in*)&ifr.ifr_addr;
        return sin->sin_addr.s_addr != 0;
    }
    
    bool configureDhcpClient(const std::string& iface) {
        // For DHCP we still need to use an external client, but we'll use fork/exec
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            execl("/sbin/dhclient", "dhclient", "-v", iface.c_str(), NULL);
            exit(1); // Exit if exec fails
        } else if (pid < 0) {
            return false;
        }
        
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
    
    bool releaseDhcpLease(const std::string& iface) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            execl("/sbin/dhclient", "dhclient", "-r", iface.c_str(), NULL);
            exit(1); // Exit if exec fails
        } else if (pid < 0) {
            return false;
        }
        
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
};

// Factory function implementation for Linux
std::unique_ptr<WifiImpl> createPlatformImpl() {
    return std::make_unique<LinuxWifiImpl>();
}

} // namespace wificpp
