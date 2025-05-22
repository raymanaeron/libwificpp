# Use this file to provide workspace-specific custom instructions to Copilot.

This is a WiFi management library project with two main components:
1. A C++ library (libwificpp) for WiFi operations using the Windows WLAN API
2. A Rust binding (wifi-rs) that provides a safe interface to the C++ library

Key files and their purposes:
- wifi_manager.hpp/cpp: Core WiFi management functionality
- wifi_types.hpp: Type definitions for WiFi-related structures
- wifi_logger.hpp/cpp: Centralized logging system
- lib.rs: Rust FFI bindings to the C++ library
