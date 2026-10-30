// Where libzrnui is: ZRNUI_LIB_DIR, else the SDK's own build tree next to
// this crate, else the system's (pkg-config's libdir, or the linker's default).
use std::path::PathBuf;
use std::process::Command;

fn main() {
    println!("cargo:rerun-if-env-changed=ZRNUI_LIB_DIR");
    let mut dir: Option<PathBuf> = std::env::var_os("ZRNUI_LIB_DIR").map(PathBuf::from);
    if dir.is_none() {
        let tree = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../libzrnui/build");
        if tree.join("libzrnui.so").exists() {
            dir = Some(tree.canonicalize().unwrap_or(tree));
        }
    }
    if dir.is_none() {
        if let Ok(out) = Command::new("pkg-config").args(["--variable=libdir", "zrnui"]).output() {
            let s = String::from_utf8_lossy(&out.stdout).trim().to_string();
            if out.status.success() && !s.is_empty() {
                dir = Some(PathBuf::from(s));
            }
        }
    }
    if let Some(d) = dir {
        println!("cargo:rustc-link-search=native={}", d.display());
        // so `cargo run` finds it without installing; a .zexec or the system sets its own path
        println!("cargo:rustc-link-arg=-Wl,-rpath,{}", d.display());
    }
    println!("cargo:rustc-link-lib=dylib=zrnui");
}
