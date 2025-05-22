#!/bin/bash
echo "Building WiFi Management Library for Linux..."
echo

# Check for required packages
check_package() {
    if ! command -v $1 &> /dev/null; then
        echo "Error: $1 is not installed."
        echo "Please install required packages with:"
        echo "  For Debian/Ubuntu: sudo apt install build-essential cmake libnl-3-dev libnl-genl-3-dev"
        echo "  For Fedora: sudo dnf install gcc-c++ cmake libnl3-devel"
        echo "  For Arch: sudo pacman -S base-devel cmake libnl"
        exit 1
    fi
}

check_package cmake
check_package g++
check_package pkg-config

# Check if Rust is installed
if ! command -v cargo &> /dev/null; then
    echo "Error: cargo is not installed. Please install Rust:"
    echo "  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh"
    exit 1
fi

# Check for required libraries
if ! pkg-config --exists libnl-3.0 libnl-genl-3.0; then
    echo "Error: Required networking libraries not found."
    echo "Please install NetLink library with:"
    echo "  For Debian/Ubuntu: sudo apt install libnl-3-dev libnl-genl-3-dev"
    echo "  For Fedora: sudo dnf install libnl3-devel"
    echo "  For Arch: sudo pacman -S libnl"
    exit 1
fi

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    mkdir build
fi

# Navigate to build directory and run CMake
echo "Step 1: Configuring C++ library with CMake..."
cd build
cmake ..
if [ $? -ne 0 ]; then
    echo "Failed to configure project with CMake."
    cd ..
    exit 1
fi

# Build the C++ library and tests
echo
echo "Step 2: Building C++ library and test applications..."
make
if [ $? -ne 0 ]; then
    echo "Failed to build C++ library."
    cd ..
    exit 1
fi

# Confirm test executable was built
if [ ! -f "test_wifi" ]; then
    echo "Warning: test_wifi was not built."
else
    echo "Successfully built test_wifi"
fi

# Return to root directory
cd ..

# Build Rust wrapper
echo
echo "Step 3: Building Rust wrapper..."
cd wifi-rs
cargo build
if [ $? -ne 0 ]; then
    echo "Failed to build Rust wrapper."
    cd ..
    exit 1
fi

# Return to root directory
cd ..

echo
echo "Build completed successfully!"
echo
echo "You can run the C++ test application with: ./build/test_wifi"
echo "You can run the Rust example with: cd wifi-rs && cargo run"

echo
read -p "Would you like to run the tests now? (y/n) " run_tests

if [ "$run_tests" = "y" ] || [ "$run_tests" = "Y" ]; then
    echo
    echo "Running C++ tests..."
    cd build
    ./test_wifi
    cd ..
    
    echo
    echo "Running Rust example..."
    cd wifi-rs
    cargo run
    cd ..
fi

echo
echo "Note: Some WiFi operations may require elevated privileges."
echo "If needed, run the test applications with: sudo ./build/test_wifi"
