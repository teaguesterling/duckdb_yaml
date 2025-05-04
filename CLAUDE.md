# CLAUDE.md - Project Notes for DuckDB YAML Extension

## Purpose of This Document

This document maintains continuity and understanding across conversation sessions about the DuckDB YAML extension project. It contains my current understanding, thoughts, questions, and ideas about the implementation. If we continue this project in a new conversation, reviewing this document will help me quickly understand the context and status of the project.

This is a high-level overview document. For detailed technical implementation notes and specific lessons learned, see CLAUDE_LESSONS.md.

## Project Overview

We are implementing a YAML extension for DuckDB, similar to the existing JSON extension, but using yaml-cpp instead of yyjson. The extension allows users to read YAML files into DuckDB tables and query the data using SQL.

## Current Implementation Status

- [x] Basic YAML file reading with read_yaml and read_yaml_objects functions
- [x] Multi-document YAML support
- [x] Top-level sequence handling (treating sequence items as rows)
- [x] File globbing and file list support
- [x] Error handling with partial recovery of valid documents
- [x] Comprehensive parameter handling with error checking
- [x] Direct file path support (SELECT * FROM 'file.yaml')
- [x] Test coverage for all implemented features
- [x] YAML logical type and conversion functions (to/from JSON)
- [ ] Fix segfault in value_to_yaml function
- [ ] Explicit column type specification via 'columns' parameter
- [ ] Comprehensive type detection (dates, timestamps, etc.)
- [ ] Support for YAML anchors and aliases
- [ ] Stream processing for large files

## Recent Changes and Findings

1. **YAML Type Implementation**:
   - Successfully implemented YAML as a type by extending VARCHAR with an alias
   - Used `LogicalType::SetAlias("yaml")` rather than creating a new type ID
   - Registered the type and appropriate cast functions
   - This approach provides a clean integration with DuckDB's type system

2. **Cast Functions and Type Conversions**:
   - Implemented proper cast functions between YAML, JSON, and VARCHAR
   - Added special treatment for multi-document YAML
   - Consistent handling of type conversions across all functions

3. **Display Format Improvements**:
   - Implemented YAML flow format (inline representation) for display purposes
   - Block format for storage and internal processing
   - Multi-document handling for both formats

4. **Code Structure Refactoring**:
   - Created a yaml_utils namespace for utility functions
   - Improved code organization with logical sections
   - Better error handling and resource management
   - Reduced code duplication through utility functions

5. **Identified Segfault Issue**:
   - The value_to_yaml function segfaults with certain inputs
   - Issue predates our refactoring efforts
   - Needs further debugging to resolve
   - Likely related to handling of Value objects or YAML::Emitter interaction

6. **Testing Framework Challenges**:
   - Discovered SQLLogicTest framework constraints with multi-line strings
   - Addressed SQL string parsing issues by using flow-style YAML in tests
   - Found yaml-cpp's parser to be extremely resilient, handling malformed inputs
   - Adjusted test expectations for error handling given parser behavior
   - Updated error message expectations to match exact DuckDB error format

## Design Decisions

- We're using yaml-cpp for parsing YAML files
- YAML type is implemented as an alias for VARCHAR 
- Multi-document YAML is converted to JSON arrays for compatibility
- YAML output uses different formats for storage (block) vs. display (flow)
- We've structured utilities to provide consistent behavior across conversion paths
- We've prioritized robust error handling throughout the implementation

## Questions and Concerns

1. **Performance**: How will the extension handle very large YAML files? Should we implement streaming parsing?
2. **Type Detection**: The current type detection is basic. How comprehensive should it be?
3. **Anchors and Aliases**: While yaml-cpp handles these internally, should we add explicit support?
4. **Value Handling**: The value_to_yaml segfault needs investigation
5. **Integration with JSON**: How tightly should this integrate with DuckDB's JSON functionality?
6. **Parameter Validation**: How strict should we be with parameter type checking?
7. **Reader Integration**: Should read_yaml_objects return the YAML type instead of parsed types?

## Future Features to Add

1. **Stream Processing**: For large files
2. **Advanced Type Detection**: Add support for dates, timestamps, and other complex types
3. **YAML Path Expressions**: Similar to JSON path expressions
4. **YAML Modification Functions**: Allow modifying YAML structures
5. **YAML Output Functions**: Allow writing DuckDB data as YAML
6. **Parameter Validation Improvements**: Stricter type checking, duplicate detection

## Technical Notes

### YAML Type System
- The YAML type is registered as an alias for VARCHAR
- Explicit casts are defined for YAML→JSON, JSON→YAML, YAML→VARCHAR, and VARCHAR→YAML
- The cast functions handle multi-document YAML properly
- Flow format is used for display, block format for storage

### Utility Functions
- The yaml_utils namespace contains reusable functions for YAML operations
- EmitYAML and EmitYAMLMultiDoc handle single and multi-document output
- ParseYAML handles both single and multi-document input
- YAMLNodeToJSON converts YAML to JSON with proper type handling
- Configuration utilities standardize emitter behavior

### Known Issues
- ValueToYAML function segfaults with some input values
- This requires further debugging with simplified test cases
- We've created a defensive implementation plan to isolate the issue

## Update Log

- Initial version: Created during first conversation about simplifying the implementation
- Update 1: Added observations about the prompter, expanded technical notes about yaml-cpp integration, and added more detail on design decisions
- Update 2: Updated based on implementation progress - added information about test file organization, DuckDB-specific syntax for structs and lists, handling of top-level sequences, and added potential future features
- Update 3: Updated after implementing robust parameter handling and error handling - marked completed tasks, updated current status, and refined next steps
- Update 4: Updated with findings from our implementation of file globbing and file list support, improved error recovery, and modernizing the code with C++ best practices
- Update 5: Added testing findings regarding parameter validation, error messages, duplicate parameters, and idiomatic C++ usage. Updated with planned improvements that have been documented in TODO.md for future implementation
- Update 6: Added detailed information about direct file path support implementation, including technical details, integration with DuckDB's file extension system, and observations about limitations and future possibilities
- Update 7: Fixed API compatibility issues with direct file path implementation. Replaced complex TableFunctionRef/DBConfig approach with simpler FileSystem::RegisterSubstrait method. Updated documentation and tests to reflect actual capabilities.
- Update 8: Restructured documentation approach to use CLAUDE.md for high-level project continuity and CLAUDE_LESSONS.md for detailed technical implementation notes.
- Update 9: After reviewing PR #4 for direct file reading implementation, noted important differences from my original approach
- Update 10: Added detailed implementation of YAML type using VARCHAR alias with SetAlias(), including cast functions for proper type conversion between YAML, JSON, and VARCHAR. Resolved issues with type registration and implemented proper multi-document handling.
- Update 11: Refactored code to use a yaml_utils namespace with improved utility functions. Implemented flow format for display and block format for storage. Identified and began debugging segfault in value_to_yaml function.
- Update 12: Discovered and resolved issues with DuckDB SQLLogicTest framework handling multi-line YAML strings. Modified test approach to use flow style YAML and updated expectations for error handling given yaml-cpp's resilient parser.
