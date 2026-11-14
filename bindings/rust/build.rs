fn main() {
    println!("cargo:rerun-if-env-changed=OPENQR_LIB_DIR");
    if let Some(directory) = std::env::var_os("OPENQR_LIB_DIR") {
        println!(
            "cargo:rustc-link-search=native={}",
            std::path::PathBuf::from(directory).display()
        );
    }
    println!("cargo:rustc-link-lib=dylib=openqr");
}
