#!/bin/bash
echo "Building WiFi Management Library for macOS..."
echo

# Check for required tools
if ! command -v cmake &> /dev/null; then
    echo "Error: cmake is not installed. Please install it using Homebrew:"
    echo "  brew install cmake"
    exit 1
fi

if ! command -v clang++ &> /dev/null; then
    echo "Error: clang++ is not installed. Please install Xcode command line tools:"
    echo "  xcode-select --install"
    exit 1
fi

if ! command -v cargo &> /dev/null; then
    echo "Error: cargo is not installed. Please install Rust:"
    echo "  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh"
    exit 1
fi

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    mkdir build
fi

# Navigate to build directory and run CMake
echo "Step 1: Configuring C++ library with CMake..."
cd build
cmake .. -DCMAKE_CXX_COMPILER=clang++
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
