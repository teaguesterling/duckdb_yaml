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
- [ ] Explicit column type specification via 'columns' parameter
- [ ] Comprehensive type detection (dates, timestamps, etc.)
- [ ] Support for YAML anchors and aliases
- [ ] Stream processing for large files
- [ ] Support for globbing multiple files

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