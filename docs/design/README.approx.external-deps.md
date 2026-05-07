# Approx External Dependencies

This note records how the experimental standalone approximation backends find
their optional external dependencies in a clean checkout.

## Scope
- Applies to the standalone tool
  [src/problog_graph_query.cpp](../../src/problog_graph_query.cpp).
- Covers optional direct-library paths for:
  - `ApproxMC`
  - `pepin`
  - `Schlandals`
- Does not make these libraries mandatory for the normal Souffle build.

## Dependency Policy
- The main repo does not vendor external solver source trees.
- The local research repo is a separate checkout under `research/`.
- Heavy third-party source trees and build outputs live under
  `research/external/`, which is ignored by the research repo.
- A clean source checkout should still configure and build
  `souffle-problog-graph-query` without these direct libraries.
- Missing direct libraries should disable the direct path, not silently change
  backend semantics.

This split is intentional: the approximation branch owns the integration code,
while solver artifacts remain reproducible local dependencies.

## Expected Layout

From the repo root:

```text
research/
  external/
    pepin/                  # optional DNF counter source checkout
    build/pepin/            # optional pepin CMake build
    schlandals/             # optional Horn/PWMC solver source checkout
    build/approxmc-v6/      # optional ApproxMC CLI build used by research notes
    install/approxmc-v6/    # optional ApproxMC install prefix
```

The clean approx clone should normally be:

```text
/home/hugh/research/datalog/souffle-approx-clean
```

## CMake Detection

The standalone target is configured in
[src/CMakeLists.txt](../../src/CMakeLists.txt).

### ApproxMC
- Detection uses `find_package(approxmc CONFIG QUIET)`.
- Direct mode also requires CMake targets for:
  - `cryptominisat5`
  - `sbva`
  - `arjun`
- If found, the tool defines `SOUFFLE_STANDALONE_HAS_APPROXMC_LIB=1` and calls
  `ApproxMC::AppMC` directly.
- If not found, `--backend amc` uses the CLI path and requires
  `APPROXMC_BIN` or `--approxmc-bin`.

Typical configure command when an installed ApproxMC stack exists:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/.local/approxmc-stack"
```

Historical ApproxMC version-reproduction notes are in the separate research
repo:

```text
research/outputs/short_term/APPROXMC_REPRO_CHEATSHEET.md
research/outputs/short_term/APPROXMC_REBUILD_ONLY.md
research/outputs/short_term/APPROXMC_RUN_ONLY.md
```

### pepin
- Detection uses `find_package(pepin CONFIG QUIET)`.
- The CMake search path explicitly includes:
  - `research/external/build/pepin`
- If found, the tool defines `SOUFFLE_STANDALONE_HAS_PEPIN_LIB=1` and calls the
  `pepin` C++ API directly.
- If not found, `--backend pepin` uses the CLI path and requires `PEPIN_BIN` or
  `--pepin-bin`.

Bootstrap source and build:

```bash
sh/setup/setup_approx_external_deps.sh --pepin
cmake -S . -B build
cmake --build build --target souffle-problog-graph-query -j4
```

### Schlandals
- The repo contains a small Rust staticlib wrapper at
  [src/problog/approx/schlandals_ffi](../../src/problog/approx/schlandals_ffi).
- The wrapper depends on a local Schlandals checkout at:
  - `research/external/schlandals`
- If Cargo and the Schlandals source checkout are available, CMake builds the
  wrapper and defines `SOUFFLE_STANDALONE_HAS_SCHLANDALS_LIB=1`.
- The CMake custom target builds with `cargo build --offline`, so the first
  setup should fetch Cargo dependencies before relying on the CMake build.
- If not found, `--backend schlandals` uses the CLI path and requires
  `SCHLANDALS_BIN` or `--schlandals-bin`.

Bootstrap source and Cargo cache:

```bash
sh/setup/setup_approx_external_deps.sh --schlandals
cmake -S . -B build
cmake --build build --target souffle-problog-graph-query -j4
```

## Helper Script

Use:

```bash
sh/setup/setup_approx_external_deps.sh --check
sh/setup/setup_approx_external_deps.sh --pepin
sh/setup/setup_approx_external_deps.sh --schlandals
sh/setup/setup_approx_external_deps.sh --all
```

The script is intentionally conservative:

- it only writes under the current repo's `research/external/` and the local
  Schlandals FFI `target/` directory
- it does not run `sudo`
- it does not delete existing external checkouts
- it does not try to rebuild the full ApproxMC dependency stack

## Reconfigure After Setup

After adding or rebuilding external deps, rerun CMake from a clean configure
step or the existing build directory:

```bash
cmake -S . -B build
cmake --build build --target souffle-problog-graph-query -j4
```

Expected configure messages when deps are missing:

```text
Building souffle-problog-graph-query without direct ApproxMC library: ...
Building souffle-problog-graph-query without direct pepin library: ...
Building souffle-problog-graph-query without direct Schlandals library: ...
```

These messages mean the direct library path is disabled for that solver. They
are not build failures.

## Source references
- [src/CMakeLists.txt](../../src/CMakeLists.txt)
- [src/problog_graph_query.cpp](../../src/problog_graph_query.cpp)
- [src/problog/approx/schlandals_ffi/Cargo.toml](../../src/problog/approx/schlandals_ffi/Cargo.toml)
- [docs/design/README.approx.pipeline.md](README.approx.pipeline.md)

## Related commits
- `17ae5e255` — docs(approx): document external dependency setup
- `a862a74c2` — docs(approx): refresh clean-clone pipeline references
- `f16853459` — build(approx): add demo tools and schlandals ffi sources
