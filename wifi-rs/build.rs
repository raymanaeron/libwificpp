use std::fs;
use std::path::Path;
use cmake::Config;

fn main() {
    // Build using cmake
    let dst = Config::new("../")
        .build_target("wificpp")
        .build();
    
    println!("cargo:rustc-link-search=native={}/build", dst.display());
    println!("cargo:rustc-link-lib=dylib=wificpp");
    
    // Also add the libwificpp directory directly
    println!("cargo:rustc-link-search=native=../build");
    
    if cfg!(target_os = "windows") {
        println!("cargo:rustc-link-lib=dylib=wlanapi");
        
        // Copy the DLL to the target directory for runtime linking
        copy_dll();
    }
}

fn copy_dll() {
    // Source DLL path
    let src_path = Path::new("../build/libwificpp.dll");
    
    // Get target directory from environment variables
    let out_dir = std::env::var("OUT_DIR").unwrap();
    let target_dir = Path::new(&out_dir).ancestors().nth(3).unwrap();
    
    // Target DLL path
    let dst_path = Path::new(&target_dir).join("libwificpp.dll");
    
    // Copy the DLL
    println!("Copying DLL from {:?} to {:?}", src_path, dst_path);
    if let Err(e) = fs::copy(src_path, dst_path) {
        println!("Failed to copy DLL: {}", e);
    }
}
