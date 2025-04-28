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
- [ ] Explicit column type specification via 'columns' parameter
- [ ] Comprehensive type detection (dates, timestamps, etc.)
- [ ] Support for YAML anchors and aliases
- [ ] Stream processing for large files

## Type System

- [ ] YAML logical type
- [ ] YAML to JSON conversion
- [ ] JSON to YAML conversion
- [ ] Value to YAML conversion
- [ ] Inline YAML parsing via `yaml()` function

## Additional Features

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
- [ ] Comprehensive user guide
- [ ] API reference
- [ ] Example gallery
- [ ] Performance benchmarks

## Known Issues and Planned Improvements

### Parameter Validation
- [ ] Stricter type checking for parameters (e.g., rejecting string values for boolean parameters)
- [ ] Explicit detection and handling of duplicate parameters
- [ ] Standardized error messages matching DuckDB conventions

### Memory Management and C++ Idioms
- [ ] Replace buffer allocation with idiomatic string handling
- [ ] Use consistent smart pointer patterns throughout the codebase
- [ ] Apply more const references to avoid unnecessary copying
- [ ] Follow DuckDB coding conventions more consistently

### Error Handling Refinements
- [ ] Ensure consistent error message formats
- [ ] Add more detailed context in error messages
- [ ] Improve granularity of error recovery

### Test Coverage
- [ ] Add tests for parameter type validation
- [ ] Add tests for duplicate parameter detection
- [ ] Expand tests for error recovery in various scenarios
