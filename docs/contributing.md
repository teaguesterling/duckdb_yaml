# Contributing

Thank you for your interest in contributing to the DuckDB YAML Extension!

## Getting Started

### Prerequisites

- C++ compiler (GCC 9+, Clang 11+, or MSVC 2019+)
- CMake 3.11+
- Git
- VCPKG (for dependency management)

### Setting Up the Development Environment

1. **Clone the repository**

    ```bash
    git clone --recurse-submodules https://github.com/teaguesterling/duckdb_yaml
    cd duckdb_yaml
    ```

2. **Set up VCPKG**

    ```bash
    # If not already installed
    cd ..
    git clone https://github.com/Microsoft/vcpkg.git
    sh ./vcpkg/scripts/bootstrap.sh -disableMetrics
    export VCPKG_TOOLCHAIN_PATH=$(pwd)/vcpkg/scripts/buildsystems/vcpkg.cmake
    cd duckdb_yaml
    ```

3. **Build the extension**

    ```bash
    make
    ```

4. **Run tests**

    ```bash
    make test
    ```

### Build Outputs

After building:

```
./build/release/duckdb                   # DuckDB shell with extension
./build/release/test/unittest            # Test runner
./build/release/extension/yaml/yaml.duckdb_extension  # Extension binary
```

## Development Workflow

### Making Changes

1. Create a branch for your changes

    ```bash
    git checkout -b feature/my-feature
    ```

2. Make your changes

3. Build and test

    ```bash
    make
    make test
    ```

4. Commit and push

    ```bash
    git add .
    git commit -m "Description of changes"
    git push origin feature/my-feature
    ```

5. Open a Pull Request

### Code Style

- Follow existing code patterns in the codebase
- Use meaningful variable and function names
- Add comments for complex logic
- Keep functions focused and reasonably sized

### Testing

#### SQL Tests

Tests are located in `test/sql/`. Create `.test` files using DuckDB's SQL test format:

```sql
# name: test/sql/yaml/my_test.test
# description: Test description
# group: [yaml]

statement ok
LOAD yaml;

query I
SELECT yaml_extract('{name: John}'::YAML, '$.name');
----
John
```

#### Running Specific Tests

```bash
# Run all tests
make test

# Run specific test file
./build/release/test/unittest --test-dir ../../.. "test/sql/yaml/my_test.test"
```

## Project Structure

```
duckdb_yaml/
├── src/                    # Source code
│   ├── yaml_extension.cpp  # Extension entry point
│   ├── yaml_functions/     # Function implementations
│   └── yaml_reader/        # File reading logic
├── test/
│   ├── sql/               # SQL tests
│   └── yaml/              # Test YAML files
├── docs/                  # Documentation (MkDocs)
├── duckdb/               # DuckDB submodule
└── extension-ci-tools/   # CI tooling
```

## Areas for Contribution

### Good First Issues

- Documentation improvements
- Additional test cases
- Bug fixes
- Performance improvements

### Feature Ideas

- Additional extraction functions
- YAML path wildcards
- Schema validation functions
- Performance optimizations
- Streaming support for large files

## Reporting Issues

When reporting issues, please include:

1. DuckDB version
2. Extension version
3. Operating system
4. Minimal reproducible example
5. Expected vs actual behavior

## Pull Request Guidelines

- Keep PRs focused on a single change
- Include tests for new functionality
- Update documentation as needed
- Ensure all tests pass
- Follow the existing code style

## License

This project is released under the MIT License. By contributing, you agree that your contributions will be licensed under the same license.

## Questions?

Feel free to open an issue for questions or discussion about potential contributions.
