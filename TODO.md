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
- [x] Value to YAML conversion (initial implementation)
- [x] Fix segfault in value_to_yaml function with debug mode implementation
- [x] Improve tests for multi-line YAML strings (resolved using flow-style)
- [x] Explicit column type specification via 'columns' parameter
- [x] Complete YAML scalar function suite (13 functions with 59 test assertions)
- [x] COPY TO YAML functionality with style and layout parameters
- [x] Comprehensive type detection (dates, timestamps, numeric sizing, booleans, special values)
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

- [x] Enhance read_yaml_objects to optionally return YAML type
- [x] Column specification for complex YAML structures
- [ ] Schema extraction helpers
- [x] YAML validation functions (yaml_valid)
- [x] YAML extraction functions (yaml_extract, yaml_extract_string, yaml_exists, yaml_type)
- [ ] YAML path expressions
- [ ] YAML modification functions
- [x] YAML output functions (COPY TO with FORMAT YAML)

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
- [x] Add more comprehensive tests (59 assertions for scalar functions)
- [ ] Optimize memory usage
- [ ] Improve performance for large files

## Known Issues and Planned Improvements

### Critical Issues

**NULL Handling Inconsistency** ✅ RESOLVED:
- ✅ Fixed YAML functions to match JSON functions in null handling behavior
- ✅ JSON: `json_extract(obj, '$.nonexistent')` → SQL NULL
- ✅ YAML: `yaml_extract(obj, '$.nonexistent')` → SQL NULL (now consistent!)
- ✅ Achieved consistency with DuckDB's JSON extension
- ✅ Fixed: yaml_extract, yaml_type (both now return SQL NULL for nonexistent paths)
- ✅ Restored proper SQL NULL semantics for missing data

### Type System
- [x] Fix segfault in value_to_yaml function (critical, affects yaml_types.test and yaml_emitter.test)
- [x] Implement a safe fallback for value_to_yaml to avoid crashes
- [x] Improve type detection for specialized formats (timestamps, dates, times)
- [x] Enhanced numeric type detection (TINYINT, SMALLINT, INTEGER, BIGINT sizing)
- [x] Case-insensitive boolean detection with multiple formats
- [x] Special numeric value handling (inf, -inf, nan)
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
- [ ] Update yaml_anchor_alias.test to use flow-style YAML for SQL parsing compatibility
- [ ] Fix yaml_types_db_integration.test column count issues
- [ ] Update yaml_integration.test to properly load JSON extension
- [ ] Add tests for error recovery in various scenarios
