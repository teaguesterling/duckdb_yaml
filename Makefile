PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Configuration of extension
EXT_NAME=yaml
EXT_CONFIG=${PROJ_DIR}extension_config.cmake

# Add explicit CMAKE_PREFIX_PATH to find yaml-cpp and correct VCPKG path
EXT_FLAGS=-DCMAKE_PREFIX_PATH="${PROJ_DIR}vcpkg_installed/arm64-osx;${PROJ_DIR}vcpkg_installed/arm64-osx/share/yaml-cpp"
VCPKG_TOOLCHAIN_PATH=/Users/pdoster/code/github.com/microsoft/vcpkg/scripts/buildsystems/vcpkg.cmake

# Include the Makefile from extension-ci-tools
include extension-ci-tools/makefiles/duckdb_extension.Makefile