//! Compiles the hace-corba C runtime (CDR core + portable HAL pool + the
//! Linux HAL backend) and links it into this crate.
//!
//! Deliberately does not depend on the `cc` crate: this project keeps its
//! build-time dependency surface small and avoids assuming registry
//! access is available, so this just shells out to `cc`/`ar` directly.
//! Only the Linux HAL backend is built — this crate targets host
//! development, matching runtime/CMakeLists.txt's default.

use std::env;
use std::path::{Path, PathBuf};
use std::process::Command;

fn run(mut cmd: Command, what: &str) {
    let status = cmd
        .status()
        .unwrap_or_else(|e| panic!("failed to invoke {what}: {e}"));
    assert!(status.success(), "{what} failed with status {status}");
}

fn main() {
    let out_dir = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR not set"));
    let manifest_dir =
        PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR not set"));

    // bindings/rust/hace-corba/ -> repo root -> runtime/
    let runtime_root = manifest_dir
        .join("../../../runtime")
        .canonicalize()
        .expect(
            "could not locate runtime/ two directories above bindings/rust/hace-corba; \
             this crate must live at bindings/rust/hace-corba under the hace-corba repo root",
        );
    let include_dir = runtime_root.join("include");

    let sources = [
        runtime_root.join("src/cdr/cdr.c"),
        runtime_root.join("src/giop/giop.c"),
        runtime_root.join("src/uiop/uiop.c"),
        runtime_root.join("src/corba/corba_exception.c"),
        runtime_root.join("src/corba/corba_ior.c"),
        runtime_root.join("src/corba/corba_poa.c"),
        runtime_root.join("src/hal/hal_pool.c"),
        runtime_root.join("src/hal/linux/hal_linux.c"),
    ];

    let mut objects: Vec<PathBuf> = Vec::new();
    for src in &sources {
        let stem = src.file_stem().unwrap().to_string_lossy();
        let obj = out_dir.join(format!("{stem}.o"));

        let mut cc = Command::new("cc");
        cc.args(["-std=c99", "-Wall", "-Wextra", "-fPIC", "-c"])
            .arg(src)
            .arg("-I")
            .arg(&include_dir)
            .arg("-o")
            .arg(&obj);
        run(cc, "cc (compiling hace-corba runtime)");

        println!("cargo:rerun-if-changed={}", src.display());
        objects.push(obj);
    }

    let lib_path = out_dir.join("libhace_corba_core.a");
    let mut ar = Command::new("ar");
    ar.arg("rcs").arg(&lib_path).args(&objects);
    run(ar, "ar (archiving libhace_corba_core.a)");

    println!("cargo:rustc-link-search=native={}", out_dir.display());
    println!("cargo:rustc-link-lib=static=hace_corba_core");
    println!("cargo:rustc-link-lib=dylib=pthread");

    for header in ["hace/cdr.h"] {
        println!(
            "cargo:rerun-if-changed={}",
            include_dir.join(header).display()
        );
    }
    println!(
        "cargo:rerun-if-changed={}",
        Path::new(&runtime_root).join("src/hal/hal.h").display()
    );
}
