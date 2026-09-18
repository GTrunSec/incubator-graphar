// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// Portions adapted from `https://github.com/kuzudb/kuzu/blob/master/tools/rust_api/build.rs` (MIT License).

use std::path::{Path, PathBuf};
use std::{env, fs};

fn track_cpp_sources(root: &Path) {
    let mut entries = fs::read_dir(root)
        .unwrap_or_else(|error| panic!("failed to read {}: {error}", root.display()))
        .collect::<Result<Vec<_>, _>>()
        .unwrap_or_else(|error| panic!("failed to enumerate {}: {error}", root.display()));
    entries.sort_unstable_by_key(|entry| entry.path());
    for entry in entries {
        let path = entry.path();
        if path.is_dir() {
            track_cpp_sources(&path);
        } else if matches!(
            path.extension().and_then(|extension| extension.to_str()),
            Some("cc" | "cmake" | "h" | "hpp")
        ) || path
            .file_name()
            .is_some_and(|name| name == "CMakeLists.txt")
        {
            println!("cargo:rerun-if-changed={}", path.display());
        }
    }
}

fn link_libraries() -> Vec<PathBuf> {
    println!("cargo:rerun-if-env-changed=PKG_CONFIG_PATH");

    let mut include_paths = Vec::new();
    for package in ["arrow", "parquet", "arrow-dataset", "arrow-acero"] {
        let library = pkg_config::Config::new()
            .statik(false)
            .cargo_metadata(true)
            .probe(package)
            .unwrap_or_else(|error| {
                panic!(
                    "{package} development files not found via pkg-config. Set PKG_CONFIG_PATH if needed: {error}"
                )
            });
        include_paths.extend(library.include_paths);
    }
    include_paths.sort_unstable();
    include_paths.dedup();

    println!("cargo:rustc-link-lib=graphar");
    println!("cargo:rustc-link-lib=graphar_thirdparty");
    include_paths
}

fn build_ffi(bridge_file: &str, out_name: &str, source_file: &str, include_paths: &[PathBuf]) {
    let mut build = cxx_build::bridge(bridge_file);
    build.file(source_file);

    build.includes(include_paths);
    // TODO support MSVC
    build.flag("-std=c++20");
    build.flag("-fdiagnostics-color=always");

    build.compile(out_name);
}

fn build_graphar() -> Vec<PathBuf> {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("..").join("cpp");
    let mut build = cmake::Config::new(&root);

    // 1. Check if `GRAPHAR_BUILD_TYPE` is set. The value must be `Release`,
    //    `Debug` or `RelWithDebInfo`.
    // 2. Otherwise preserve Cargo's optimization contract. Custom profiles
    //    (including an optimized test profile) still report `PROFILE=debug`,
    //    so `PROFILE` alone would silently compile the C++ data path without
    //    optimization.
    let cmake_build_type = env::var("GRAPHAR_BUILD_TYPE").unwrap_or_else(|_| {
        if env::var("PROFILE").as_deref() == Ok("release") {
            "Release".to_string()
        } else if env::var("OPT_LEVEL").as_deref() == Ok("0") {
            "Debug".to_string()
        } else {
            "RelWithDebInfo".to_string()
        }
    });

    println!("cargo:rerun-if-env-changed=GRAPHAR_BUILD_TYPE");
    println!("cargo:rerun-if-env-changed=PROFILE");
    println!("cargo:rerun-if-env-changed=OPT_LEVEL");

    build
        .no_build_target(true)
        .define("CMAKE_BUILD_TYPE", cmake_build_type)
        .define("GRAPHAR_BUILD_STATIC", "ON")
        .define("GRAPHAR_ENABLE_SANITIZER", "OFF");

    println!("cargo:rerun-if-env-changed=CMAKE_PREFIX_PATH");
    let mut cmake_prefixes = env::var_os("CMAKE_PREFIX_PATH")
        .map(|prefixes| env::split_paths(&prefixes).collect::<Vec<_>>())
        .unwrap_or_default();
    if let Ok(prefix) = pkg_config::get_variable("arrow", "prefix") {
        let prefix = PathBuf::from(prefix);
        if !cmake_prefixes.contains(&prefix) {
            cmake_prefixes.push(prefix);
        }
    }
    if !cmake_prefixes.is_empty() {
        let cmake_prefix_path = cmake_prefixes
            .iter()
            .map(|prefix| prefix.to_string_lossy())
            .collect::<Vec<_>>()
            .join(";");
        build.define("CMAKE_PREFIX_PATH", cmake_prefix_path);
    }
    println!("cargo:rerun-if-env-changed=PKG_CONFIG_PATH");
    let build_dir = build.build();

    let lib_path = build_dir.join("build");
    let src_lib_path = lib_path.join("src");
    println!("cargo:rustc-link-search=native={}", lib_path.display());
    println!("cargo:rustc-link-search=native={}", src_lib_path.display());

    println!("cargo:rerun-if-changed=include/graphar_rs.h");
    println!("cargo:rerun-if-changed=src/graphar_rs.cc");
    track_cpp_sources(&root);

    // Include `cpp/src` and `thirdparty`
    vec![root.join("src/"), root.join("thirdparty/")]
}

fn main() {
    // Include `include/`
    let mut include_paths = vec![Path::new(env!("CARGO_MANIFEST_DIR")).join("include")];
    include_paths.extend(build_graphar());
    include_paths.extend(link_libraries());

    // Build files generated by `ffi.rs` and `graphar_rs.cc`
    build_ffi(
        "src/ffi.rs",
        "graphar_cxx",
        "src/graphar_rs.cc",
        &include_paths,
    );
}
