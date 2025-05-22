@echo off
REM Add the DLL directory to the PATH temporarily
SET PATH=%PATH%;%~dp0build

REM Run the Rust example
cd wifi-rs
cargo run
