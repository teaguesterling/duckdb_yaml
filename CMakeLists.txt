cmake_minimum_required(VERSION 3.5)

# Set extension name here
set(TARGET_NAME yaml)

# DuckDB's extension distribution supports vcpkg. As such, dependencies can be added in ./vcpkg.json and then
# used in cmake with find_package. Feel free to remove or replace with other dependencies.
# Note that it should also be removed from vcpkg.json to prevent needlessly installing it..
find_package(yaml-cpp REQUIRED)

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})
include_directories(src/include)

set(EXTENSION_SOURCES
  src/yaml_extension.cpp
  src/yaml_reader.cpp
  src/yaml_types.cpp
  src/yaml_debug.cpp
  src/yaml_column_types.cpp
  src/yaml_scalar_functions.cpp
  src/yaml_extraction_functions.cpp
  src/yaml_copy.cpp
  src/yaml_utils.cpp
  src/yaml_formatting.cpp
)

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

# Link OpenSSL in both the static library as the loadable extension
target_link_libraries(${EXTENSION_NAME} yaml-cpp::yaml-cpp)
target_link_libraries(${LOADABLE_EXTENSION_NAME} yaml-cpp::yaml-cpp)

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
