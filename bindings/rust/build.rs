use std::env;
use std::path::PathBuf;

fn main() {
    let project_root = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap())
        .join("../..");
    let project_root = project_root.canonicalize()
        .expect("Failed to find PlumbrC project root");

    // Compile all C sources into a static lib
    let mut build = cc::Build::new();
    build
        .include(project_root.join("include"))
        .include(project_root.join("src/amd"))
        .define("_GNU_SOURCE", None)
        .define("NDEBUG", None)
        .flag("-std=c11")
        .flag("-O3")
        .flag("-march=native")
        .opt_level(3);

    // Core sources
    let sources = [
        "src/aho_corasick.c",
        "src/arena.c",
        "src/hwdetect.c",
        "src/io.c",
        "src/libplumbr.c",
        "src/parallel.c",
        "src/patterns.c",
        "src/pipeline.c",
        "src/redactor.c",
    ];

    for src in &sources {
        build.file(project_root.join(src));
    }

    // AMD SIMD sources
    let amd_sources = [
        "src/amd/avx2_scan.c",
        "src/amd/sse42_filter.c",
    ];
    for src in &amd_sources {
        let mut amd_build = cc::Build::new();
        amd_build
            .include(project_root.join("include"))
            .include(project_root.join("src/amd"))
            .define("_GNU_SOURCE", None)
            .define("NDEBUG", None)
            .flag("-std=c11")
            .flag("-O3")
            .flag("-march=native")
            .flag("-mavx2")
            .opt_level(3)
            .file(project_root.join(src));
        amd_build.compile(&format!("plumbr_amd_{}", src.replace('/', "_").replace('.', "_")));
    }

    build.compile("plumbr_core");

    // Link dependencies
    println!("cargo:rustc-link-lib=pcre2-8");
    println!("cargo:rustc-link-lib=pthread");

    // Rerun if C sources change
    println!("cargo:rerun-if-changed={}", project_root.join("src").display());
    println!("cargo:rerun-if-changed={}", project_root.join("include").display());
}
