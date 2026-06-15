# This file is included by DuckDB's build system. It specifies which extension to load

# Extension from this repo
duckdb_extension_load(yaml
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}
    LOAD_TESTS
    # WASM side modules are linked by a separate `emcc -sSIDE_MODULE=2 ...
    # ${LINKED_LIBS}` step (see duckdb/extension/extension_build_tools.cmake)
    # that ignores target_link_libraries(). Without naming yaml-cpp here its
    # symbols are left as unresolved imports and the .wasm fails to load /
    # throws at first call. See test/wasm/ and issue #40.
    LINKED_LIBS "$<TARGET_FILE:yaml-cpp::yaml-cpp>"
)

# Any extra extensions that should be built
# e.g.: duckdb_extension_load(json)
duckdb_extension_load(json)
