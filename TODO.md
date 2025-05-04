# YAML Extension TODO

## Core Functionality

- [x] Basic YAML file reading
- [x] File path validation
- [x] Simple type detection
- [x] Multi-document support
- [x] Top-level sequence handling
- [x] Robust parameter handling and validation
- [x] Special case handling for non-map documents
- [x] Error handling with ignore_errors parameter
- [x] File globbing support
- [x] File list support
- [x] Direct file path support (e.g., SELECT * FROM 'file.yaml')
- [x] YAML logical type implementation (as VARCHAR alias)
- [x] YAML to JSON conversion
- [x] JSON to YAML conversion
- [x] Value to YAML conversion (needs debugging)
- [ ] Fix segfault in value_to_yaml function
- [ ] Explicit column type specification via 'columns' parameter
- [ ] Comprehensive type detection (dates, timestamps, etc.)
- [ ] Stream processing for large files

## Type System

- [x] YAML logical type (implemented as VARCHAR alias)
- [x] YAML to JSON conversion
- [x] JSON to YAML conversion
- [x] Value to YAML conversion (with segfault issue)
- [x] VARCHAR to YAML conversion
- [x] YAML to VARCHAR conversion
- [ ] Improved anchor and alias support

## Additional Features

- [ ] Enhance read_yaml_objects to optionally return YAML type
- [ ] Column specification for complex YAML structures
- [ ] Schema extraction helpers
- [ ] YAML validation functions
- [ ] YAML extraction functions (similar to JSON functions)
- [ ] YAML path expressions
- [ ] YAML modification functions
- [ ] YAML output functions

## Error Handling

- [x] Basic error messages with contextual details
- [x] Configurable error recovery via ignore_errors parameter
- [x] Recovery of valid documents in partially invalid files
- [ ] Detailed error messages with line/column information
- [ ] Warnings for non-fatal issues

## Performance

- [ ] Streaming parser for large files
- [ ] Memory optimization for large documents
- [ ] Batch processing for multi-document files

## Documentation

- [x] Basic README with usage examples
- [x] Document direct file path usage
- [x] Document YAML type and conversion functions
- [ ] Comprehensive user guide
- [ ] API reference
- [ ] Example gallery
- [ ] Performance benchmarks

## Code Organization and Quality

- [x] Restructure code with utility namespace
- [x] Improve function documentation
- [x] Consistent error handling
- [x] Reduce code duplication
- [ ] Add more comprehensive tests
- [ ] Optimize memory usage
- [ ] Improve performance for large files

## Known Issues and Planned Improvements

### Type System
- [ ] Fix segfault in value_to_yaml function
- [ ] Improve type detection for specialized formats (timestamps, dates)
- [ ] Add better support for YAML anchors and aliases

### Parameter Validation
- [ ] Stricter type checking for parameters
- [ ] Explicit detection and handling of duplicate parameters
- [ ] Standardized error messages matching DuckDB conventions

### Memory Management and C++ Idioms
- [x] Replace manual string building with proper YAML::Emitter usage
- [x] Use consistent namespace organization
- [x] Apply more const references to avoid unnecessary copying
- [x] Follow DuckDB coding conventions more consistently

### Test Coverage
- [x] Add tests for YAML type functionality
- [x] Add tests for anchor/alias handling
- [x] Add tests for YAML formatting
- [ ] Add tests for error recovery in various scenarios
