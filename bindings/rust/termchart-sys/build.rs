//! Locate the native `termchart_capi` library and tell rustc where to find it.
//!
//! The C ABI library is built from the C++ repo with
//! `cmake -DTERMCHART_BUILD_CAPI=ON` (never by this crate — we don't compile C++).
//! Search order:
//!   1. `TERMCHART_CAPI_DIR` env var (absolute dir containing the .dll/.so/.lib);
//!   2. `TERMCHART_CAPI_LIB` env var (full path — its parent dir is used);
//!   3. nothing: rely on the platform default search path (`LIBRARY_PATH`, PATH, ldconfig).
//!
//! On Windows the import library is `libtermchart_capi.dll.a` (MinGW) and the runtime
//! DLL must be on PATH at run time; we cannot carry it into the crate.

use std::env;
use std::path::PathBuf;

fn main() {
    println!("cargo:rustc-link-lib=dylib=termchart_capi");

    if let Ok(dir) = env::var("TERMCHART_CAPI_DIR") {
        println!("cargo:rustc-link-search=native={dir}");
    } else if let Ok(lib) = env::var("TERMCHART_CAPI_LIB") {
        if let Some(parent) = PathBuf::from(lib).parent() {
            println!("cargo:rustc-link-search=native={}", parent.display());
        }
    }

    // Re-run if the user changes which native lib to link against.
    println!("cargo:rerun-if-env-changed=TERMCHART_CAPI_DIR");
    println!("cargo:rerun-if-env-changed=TERMCHART_CAPI_LIB");
}
