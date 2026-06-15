# WASM build tests

These tests guard against the failure mode reported in
[#40](https://github.com/teaguesterling/duckdb_yaml/issues/40): the DuckDB-WASM
build installs but fails to load (or fails at first function call) because a
dependency's symbols are left unresolved in the side module.

## Root cause

The loadable `*.duckdb_extension.wasm` is **not** produced by the normal CMake
link. DuckDB's `extension/extension_build_tools.cmake` runs a separate
`emcc -sSIDE_MODULE=2 ... ${LINKED_LIBS}` post-build step. That step links the
extension object archive against **only** the libraries named in
`LINKED_LIBS` in `extension_config.cmake`. The `target_link_libraries(...)`
calls in `CMakeLists.txt` are honored for native builds but **ignored** by this
emcc step. If `yaml-cpp` is not in `LINKED_LIBS`, its symbols become unresolved
`env` imports in the `.wasm`.

The fix is in `extension_config.cmake`:

```cmake
duckdb_extension_load(yaml
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}
    LOAD_TESTS
    LINKED_LIBS "$<TARGET_FILE:yaml-cpp::yaml-cpp>"
)
```

## Two layers of testing

### Layer 1 — static import check (fast, deterministic, no runtime)

`check_wasm_imports.mjs` parses the built `.wasm` (validate-only, **no
instantiation**) and fails if any dependency symbol is **imported but not also
defined/exported** by the module.

Subtlety that matters: emscripten side modules use interposition linking, so a
module legitimately *imports* many symbols it also *defines* (and legitimately
imports host-provided symbols — duckdb's runtime, and core deps like yyjson
that are bundled into the duckdb-wasm host). So "is it imported" is the **wrong**
question and produces false positives. The real bug is a dependency symbol that
is imported, **not** in the module's own exports, and not host-provided — i.e.
genuinely unresolved. Imports are resolved per bucket against the right export
kind: `env`/`GOT.func` → exported functions, `GOT.mem` → exported data globals
(so unresolved **vtables** `_ZTV*` / typeinfo `_ZTI*` are caught too, not just
functions). Empirically: a broken yaml build has 38 such unresolved symbols
(29 `env` functions like `YAML::Load`/`YAML::Emitter::*`/`YAML::detail::node_data::*`,
plus `GOT.func`/`GOT.mem` entries incl. `_ZTVN4YAML9ExceptionE`); the fixed
build has 0 across all kinds.

Why static, not instantiation-based:
- It catches the bug whether the missing symbol would fail at **load** time or
  only at **call** time (DuckDB-WASM side modules can load "successfully" and
  then throw on the first call that needs the symbol — see Rusty Conover's
  writeup below).
- It needs **no** matching `@duckdb/duckdb-wasm` engine and **no** duckdb
  version match, so a red result is unambiguously the LINKED_LIBS bug and not an
  ABI/version mismatch.

Run after a wasm build:

```bash
make wasm_mvp                       # or wasm_eh / wasm_threads
test/wasm/run_wasm_checks.sh        # scans build/wasm_* for *.wasm
```

Exit non-zero = forbidden dependency symbols found.

### Layer 2 — end-to-end sqllogictest against duckdb-wasm (high fidelity)

Rusty Conover's harness instantiates the published WASM engine in Node, does
`INSTALL ... FROM community; LOAD ...`, and runs the extension's existing
`test/sql/*.test` files against it. This validates that the extension actually
*works* under wasm (not just that symbols resolve), and also surfaces
version/ABI problems. It requires a published artifact + matching engine.

- Harness: https://github.com/Query-farm-haybarn/haybarn-extension-wasm-tester
- Writeup: https://rusty.today/blog/testing-duckdb-wasm-extensions/

Layer 1 is the build-time gate (runs on the artifact we just built); Layer 2 is
the post-publish / pre-release end-to-end check.

## CI

`make wasm_mvp` etc. run inside the reusable
`duckdb/extension-ci-tools` distribution workflow, which currently **builds and
uploads** the `.wasm` but never inspects or loads it — which is why this bug
shipped. The plan is to add Layer 1 as a step right after the wasm build (it
only needs `node`), and to propose the same generic step upstream so every
extension benefits.
