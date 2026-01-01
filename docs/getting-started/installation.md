# Installation

The DuckDB YAML extension can be installed in several ways depending on your needs.

## From Community Extensions (Recommended)

The simplest way to install the extension is from the DuckDB Community Extensions repository:

```sql
INSTALL yaml FROM community;
LOAD yaml;
```

This method:

- Works with the latest stable DuckDB release
- Provides signed, pre-built binaries
- Handles platform detection automatically

## From GitHub Release

You can install directly from a GitHub release:

```sql
INSTALL yaml FROM 'https://github.com/teaguesterling/duckdb_yaml/releases/download/v0.1.0/yaml.duckdb_extension';
LOAD yaml;
```

!!! note "Unsigned Extensions"
    When installing from GitHub releases, you may need to enable unsigned extensions:

    ```sql
    SET allow_unsigned_extensions = true;
    ```

## From Source

For development or to access the latest features, build from source:

### Prerequisites

- C++ compiler (GCC 9+, Clang 11+, or MSVC 2019+)
- CMake 3.11+
- Git

### Build Steps

```bash
# Clone the repository with submodules
git clone --recurse-submodules https://github.com/teaguesterling/duckdb_yaml
cd duckdb_yaml

# Build the extension
make

# Run tests
make test
```

### Using VCPKG for Dependencies

The extension uses VCPKG for dependency management (yaml-cpp):

```bash
# Set up VCPKG (if not already installed)
cd <your-working-dir>
git clone https://github.com/Microsoft/vcpkg.git
sh ./vcpkg/scripts/bootstrap.sh -disableMetrics
export VCPKG_TOOLCHAIN_PATH=$(pwd)/vcpkg/scripts/buildsystems/vcpkg.cmake

# Then build the extension
cd duckdb_yaml
make
```

### Build Outputs

After building, you'll find:

```
./build/release/duckdb                           # DuckDB shell with extension loaded
./build/release/test/unittest                    # Test runner
./build/release/extension/yaml/yaml.duckdb_extension  # Loadable extension
```

## Loading the Extension

After installation, load the extension in your DuckDB session:

```sql
LOAD yaml;
```

You can verify the extension is loaded by running:

```sql
SELECT * FROM duckdb_extensions() WHERE extension_name = 'yaml';
```

## Client-Specific Configuration

### Python

```python
import duckdb

# For community extensions
conn = duckdb.connect()
conn.execute("INSTALL yaml FROM community")
conn.execute("LOAD yaml")

# For unsigned extensions
conn = duckdb.connect(config={'allow_unsigned_extensions': 'true'})
conn.execute("LOAD 'path/to/yaml.duckdb_extension'")
```

### Node.js

```javascript
const duckdb = require('duckdb');

// For community extensions
const db = new duckdb.Database(':memory:');
db.run("INSTALL yaml FROM community");
db.run("LOAD yaml");

// For unsigned extensions
const db = new duckdb.Database(':memory:', {"allow_unsigned_extensions": "true"});
```

### CLI

```bash
# For unsigned extensions
duckdb -unsigned

# Then in the shell
LOAD 'path/to/yaml.duckdb_extension';
```

## Troubleshooting

### Extension Not Found

If you get an extension not found error:

1. Verify the extension file exists at the specified path
2. Check that you're using a compatible DuckDB version
3. Ensure `allow_unsigned_extensions` is set if required

### Platform Compatibility

The extension must be built for your specific:

- Operating system (Linux, macOS, Windows)
- Architecture (x86_64, ARM64)
- DuckDB version

Community extensions handle this automatically. For custom builds, ensure you're building for the correct platform.
