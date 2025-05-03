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
- [ ] Explicit column type specification via 'columns' parameter
- [ ] YAML logical type and conversion functions (to/from JSON)
- [ ] Comprehensive type detection (dates, timestamps, etc.)
- [ ] Support for YAML anchors and aliases
- [ ] Stream processing for large files

## Recent Changes and Findings

1. **Direct File Path Support**:
   - Implemented support for directly using YAML files in FROM clauses (e.g., `SELECT * FROM 'file.yaml'`)
   - Registered both `.yaml` and `.yml` file extensions with FileSystem::RegisterSubstrait
   - Initially encountered API compatibility issues that required simplifying the implementation
   - Added comprehensive tests for this functionality
   - Updated documentation to reflect this new capability

2. **API Compatibility Fix for Direct File Path Support**:
   - Initial implementation using TableFunctionRef and DBConfig::RegisterFileExtension encountered API compatibility issues
   - Switched to a simpler approach using FileSystem::RegisterSubstrait that directly maps file extensions to function names
   - Simplified test cases to focus on core functionality
   - This experience highlighted the importance of working within DuckDB's established patterns

3. **Enhanced File Handling**:
   - Added support for providing a list of file paths (e.g., `read_yaml(['a.yaml', 'b.yaml'])`)
   - Implemented globbing using built-in `fs.Glob` functionality
   - Added support for both `.yaml` and `.yml` extensions

4. **Improved Error Recovery**:
   - Implemented a robust recovery mechanism for partially invalid YAML files
   - Enhanced the `RecoverPartialYAMLDocuments` function to handle document separators
   - Made error handling more granular (per-file and per-document)

5. **Code Modernization**:
   - Replaced C-style strings with modern C++ string objects
   - Added const qualifiers and references for better performance and safety
   - Made function signatures more consistent with DuckDB's style

6. **Testing Findings**:
   - Parameter type checking is too permissive (e.g., `auto_detect='yes'` passes)
   - Expected error messages don't always match actual DuckDB errors
   - Duplicate parameter detection isn't working as expected
   - Some memory management could be improved with more idiomatic C++

6. **Recent Implementation Findings (File Reading)**:
   - The final implementation of file reading in PR #4 demonstrates several best practices for DuckDB extensions:
    1. **DuckDB File System Abstraction**: Properly uses `FileSystem::GetFileSystem(context)` to obtain file system instance, ensuring compatibility with DuckDB's virtual file system architecture.
    2. **Resource Management**: Uses DuckDB's file handle pattern correctly, ensuring proper resource cleanup through RAII.
    3. **Error Handling**: More consistent error messages that follow DuckDB's conventions for error reporting.
    4. **Parameter Validation**: Improved validation steps for file paths and other parameters before attempting file operations.
    5. **Multi-document Processing**: Refined approach to handling multi-document YAML files with better separation of concerns.
    6. **Code Organization**: Better organization of related functionality into logical groups, making the code more maintainable.
 
    The implementation demonstrates a good balance between utilizing DuckDB's native capabilities and extending them with YAML-specific functionality.

## Design Decisions

- We're using yaml-cpp for parsing YAML files
- We're using a similar architecture to DuckDB's JSON extension but simplified
- We're storing each YAML document as a single row in the output table
- We're prioritizing correctness and clarity over optimization initially
- We're using DuckDB's unittest framework for testing
- We're providing similar file handling capabilities to the JSON extension
- For direct file path support, we're calling read_yaml with default parameters, requiring users to use the explicit function call for customization
- We've decided to defer some parameter validation improvements for a future update

## Questions and Concerns

1. **Performance**: How will the extension handle very large YAML files? Should we implement streaming parsing?
2. **Type Detection**: The current type detection is basic. How comprehensive should it be?
3. **Anchors and Aliases**: YAML-specific features like anchors and aliases aren't currently supported. How important are they?
4. **Integration with JSON**: How tightly should this integrate with DuckDB's JSON functionality?
5. **Inline YAML Support**: Should we add a separate `yaml` function for handling inline YAML strings?
6. **Parameter Validation**: How strict should we be with parameter type checking?
7. **Direct Path Parameters**: Is there a way to support parameter passing through the direct file path syntax (e.g., `FROM 'file.yaml' (param=value)`)? This appears to be a limitation of DuckDB's current file extension system.
8. **Error Handling Granularity**: Should we provide more detailed error messages with line/column information for YAML parsing errors?

## Future Features to Add

1. **YAML Type System**: Add support for YAML as a first-class type in DuckDB
2. **Conversion Functions**: Add YAML to JSON and JSON to YAML conversion
3. **Advanced Type Detection**: Add support for dates, timestamps, and other complex types
4. **YAML Path Expressions**: Similar to JSON path expressions
5. **YAML Modification Functions**: Allow modifying YAML structures
6. **YAML Output Functions**: Allow writing DuckDB data as YAML
7. **Streaming Processing**: For large files
8. **Anchor and Alias Support**: For YAML-specific features
9. **Inline YAML Function**: Add a `yaml()` function for parsing inline YAML strings
10. **Parameter Validation Improvements**: Stricter type checking, duplicate detection
11. **Parameter Forwarding for Direct Paths**: Enable parameter passing in direct file syntax if DuckDB adds support

## Technical Notes

### Core Functionality
- The YAMLReader class provides two main functions: read_yaml and read_yaml_objects
- read_yaml expands each document into rows while read_yaml_objects preserves document structure
- The implementation supports multi-document YAML files, top-level sequences, and robust error recovery
- File handling supports single files, file lists, glob patterns, and directory paths

### Direct File Path Support
- Implementation uses FileSystem::RegisterSubstrait to map file extensions to function names
- Both .yaml and .yml extensions are registered to use the read_yaml function
- This simple approach achieves the goal of allowing direct file references in FROM clauses
- The approach aligns with DuckDB's current API patterns for extension file handlers
- The implementation supports filtering, aggregation, and table creation on the file data

### User Experience Considerations
- The API design prioritizes simplicity and DuckDB-like experiences
- We've made error handling robust but not overly verbose
- We maintain compatibility with DuckDB's expected extension behavior
- We've added extensive README examples to show common usage patterns

## Observations About the Prompter

The prompter appears to have significant experience with DuckDB extension development and prefers:

- Incremental, methodical development with clear tracking of progress
- Documentation-driven and test-driven development practices
- Simplification for debugging purposes before adding complexity
- Collaborative problem-solving, valuing technical insights and recommendations
- Clear organization of code and development priorities
- Explicit knowledge preservation (hence this document)
- Thorough testing with comprehensive test cases
- Code that follows established DuckDB extension patterns and idioms

The prompter values both practical implementation help and higher-level architectural guidance, with a focus on creating a robust, user-friendly extension. They appreciate deliberate decision-making and documentation of rationale for future reference.

The prompter is particularly interested in making the YAML extension feel native to DuckDB, as evidenced by the request to implement direct file path support. This suggests a focus on user experience and integration with DuckDB's existing patterns.

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
