# YAML Extension for DuckDB

This extension allows DuckDB to read YAML files directly into tables. It provides a simple interface for accessing YAML data within SQL queries.

## Meta Considerations

At a more meta level, this extension is an exercise in having Claude.ai create an extension for me, using other extensions as examples, while I try to delay diving into the code myself for as long as possible. The inital chat I used to start this project is [shared here](https://claude.ai/share/b7778f6e-cce4-4bba-9809-8682468a6940). Eventually, I created a project with integrations into this GitHub repository, which prevents additional sharing, but I will add the chats to this repository for reference.

## Features

- Read YAML files into DuckDB tables
- Auto-detect data types from YAML content
- Support for multi-document YAML files
- Robust error handling

## Usage

### Loading the Extension

```sql
LOAD yaml;

-- Read from a file
SELECT * FROM read_yaml('path/to/file.yaml');

-- Read from a literal string
SELECT * FROM read_yaml('
name: John
age: 30
');

-- Auto-detect types (default: true)
SELECT * FROM read_yaml('file.yaml', auto_detect=true);

-- Handle multiple YAML documents (default: true)
SELECT * FROM read_yaml('file.yaml', multi_document=true);

-- Ignore parsing errors (default: false)
SELECT * FROM read_yaml('file.yaml', ignore_errors=true);

-- Set maximum file size in bytes (default: 16MB)
SELECT * FROM read_yaml('file.yaml', maximum_object_size=1048576);

-- Access scalar values
SELECT yaml->name, yaml->age FROM yaml_table;

-- Access array elements
SELECT yaml->tags[0], yaml->tags[1] FROM yaml_table;

-- Access nested structures
SELECT yaml->person->addresses[0]->city FROM yaml_table;

```

## Limitations

This is an alpha version with the following limitations:

 - Limited type detection
 - No support for YAML anchors and aliases
 - No YAML to JSON conversion

## Building

### Managing dependencies
DuckDB extensions uses VCPKG for dependency management. Enabling VCPKG is very simple: follow the [installation instructions](https://vcpkg.io/en/getting-started) or just run the following:
```shell
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
export VCPKG_TOOLCHAIN_PATH=`pwd`/vcpkg/scripts/buildsystems/vcpkg.cmake
```
Note: VCPKG is only required for extensions that want to rely on it for dependency management. If you want to develop an extension without dependencies, or want to do your own dependency management, just skip this step. Note that the example extension uses VCPKG to build with a dependency for instructive purposes, so when skipping this step the build may not work without removing the dependency.

### Build steps
Now to build the extension, run:
```sh
make
```
The main binaries that will be built are:
```sh
./build/release/duckdb
./build/release/test/unittest
./build/release/extension/yaml/yaml.duckdb_extension
```
- `duckdb` is the binary for the duckdb shell with the extension code automatically loaded.
- `unittest` is the test runner of duckdb. Again, the extension is already linked into the binary.
- `yaml.duckdb_extension` is the loadable binary as it would be distributed.

## Running the tests
Different tests can be created for DuckDB extensions. The primary way of testing DuckDB extensions should be the SQL tests in `./test/sql`. These SQL tests can be run using:
```sh
make test
```

