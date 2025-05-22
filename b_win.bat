@echo off
echo Building WiFi Management Library...
echo.

REM Set environment variables if they don't exist
if not defined MINGW_PATH (
  echo MINGW_PATH not set, using default C:\Qt\Tools\mingw1310_64\bin
  set MINGW_PATH=C:\Qt\Tools\mingw1310_64\bin
)

if not defined CMAKE_PATH (
  echo CMAKE_PATH not set, using default "C:\Program Files\CMake\bin"
  set CMAKE_PATH="C:\Program Files\CMake\bin"
)

REM Add MinGW and CMake to PATH temporarily
set PATH=%MINGW_PATH%;%CMAKE_PATH%;%PATH%

REM Create build directory if it doesn't exist
if not exist build mkdir build

REM Navigate to build directory and run CMake
echo Step 1: Configuring C++ library with CMake...
cd build
cmake -G "MinGW Makefiles" ..
if %ERRORLEVEL% neq 0 (
  echo Failed to configure project with CMake.
  cd ..
  exit /b %ERRORLEVEL%
)

REM Build the C++ library and tests
echo.
echo Step 2: Building C++ library and test applications...
mingw32-make
if %ERRORLEVEL% neq 0 (
  echo Failed to build C++ library.
  cd ..
  exit /b %ERRORLEVEL%
)

REM Confirm test executable was built
if not exist test_wifi.exe (
  echo Warning: test_wifi.exe was not built.
) else (
  echo Successfully built test_wifi.exe
)

REM Return to root directory
cd ..

REM Build Rust wrapper
echo.
echo Step 3: Building Rust wrapper...
cd wifi-rs
cargo build
if %ERRORLEVEL% neq 0 (
  echo Failed to build Rust wrapper.
  cd ..
  exit /b %ERRORLEVEL%
)

REM Return to root directory
cd ..

echo.
echo Build completed successfully!
echo.
echo You can run the C++ test application with: .\build\test_wifi.exe
echo You can run the Rust example with: cd wifi-rs ^&^& cargo run

echo.
echo Would you like to run the tests now? (Y/N)
set /p run_tests=

if /i "%run_tests%"=="Y" (
  echo.
  echo Running C++ tests...
  cd build
  .\test_wifi.exe
  cd ..
  
  echo.
  echo Running Rust example...
  cd wifi-rs
  cargo run
  cd ..
)
