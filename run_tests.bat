@echo off
echo Running WiFi Management Library Tests...
echo.

echo Step 1: Running C++ test application...
if not exist build\test_wifi.exe (
  echo Error: build\test_wifi.exe not found.
  echo Please run build_all.bat first to build the project.
  exit /b 1
)

cd build
.\test_wifi.exe
if %ERRORLEVEL% neq 0 (
  echo C++ test application failed with error code %ERRORLEVEL%.
  cd ..
  exit /b %ERRORLEVEL%
)
cd ..

echo.
echo Step 2: Running Rust example...
if not exist wifi-rs\target (
  echo Error: Rust project hasn't been built.
  echo Please run build_all.bat first to build the project.
  exit /b 1
)

cd wifi-rs
cargo run
if %ERRORLEVEL% neq 0 (
  echo Rust example failed with error code %ERRORLEVEL%.
  cd ..
  exit /b %ERRORLEVEL%
)
cd ..

echo.
echo All tests completed successfully!
