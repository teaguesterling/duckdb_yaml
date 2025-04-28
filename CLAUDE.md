# CLAUDE.md - Project Notes for DuckDB YAML Extension

## Purpose of This Document

This document is written by Claude to maintain continuity and understanding across conversation sessions about the DuckDB YAML extension project. It contains my current understanding, thoughts, questions, and ideas about the implementation. If we continue this project in a new conversation, reviewing this document will help me quickly understand the context and status of the project.

This is a candid internal document to help me be maximally helpful throughout the development process. It includes my observations about the prompter's preferences and approach to better align my assistance with their needs.

## Project Overview

We are implementing a YAML extension for DuckDB, similar to the existing JSON extension, but using yaml-cpp instead of yyjson. The extension allows users to read YAML files into DuckDB tables and query the data using SQL.

## Current Implementation Status

- We've created a solid implementation of the YAML reader functionality
- The implementation includes the YAMLReader class with read_yaml and read_yaml_objects functions
- The read_yaml function creates a table with multiple rows from YAML documents
- The read_yaml_objects function maintains document structure as a single row per document
- Multi-document YAML support is fully implemented
- Top-level sequence handling is implemented, treating sequence items as rows
- File globbing and file list support is implemented
- Improved error handling with partial recovery of valid documents in files with errors
- Comprehensive parameter handling with error checking
- Test coverage for basic functionality and error handling

## Recent Changes and Findings

1. **Enhanced File Handling**:
   - Added support for providing a list of file paths (e.g., `read_yaml(['a.yaml', 'b.yaml'])`)
   - Implemented globbing using built-in `fs.Glob` functionality
   - Added support for both `.yaml` and `.yml` extensions

2. **Improved Error Recovery**:
   - Implemented a robust recovery mechanism for partially invalid YAML files
   - Enhanced the `RecoverPartialYAMLDocuments` function to handle document separators
   - Made error handling more granular (per-file and per-document)

3. **Code Modernization**:
   - Replaced C-style strings with modern C++ string objects
   - Added const qualifiers and references for better performance and safety
   - Made function signatures more consistent with DuckDB's style

4. **Testing Findings**:
   - Parameter type checking is too permissive (e.g., `auto_detect='yes'` passes)
   - Expected error messages don't always match actual DuckDB errors
   - Duplicate parameter detection isn't working as expected
   - Some memory management could be improved with more idiomatic C++

## Design Decisions

- We're using yaml-cpp for parsing YAML files
- We're using a similar architecture to DuckDB's JSON extension but simplified
- We're storing each YAML document as a single row in the output table
- We're prioritizing correctness and clarity over optimization initially
- We're using DuckDB's unittest framework for testing
- We're providing similar file handling capabilities to the JSON extension
- We've decided to defer some parameter validation improvements for a future update

## Questions and Concerns

1. **Performance**: How will the extension handle very large YAML files? Should we implement streaming parsing?
2. **Type Detection**: The current type detection is basic. How comprehensive should it be?
3. **Anchors and Aliases**: YAML-specific features like anchors and aliases aren't currently supported. How important are they?
4. **Integration with JSON**: How tightly should this integrate with DuckDB's JSON functionality?
5. **Inline YAML Support**: Should we add a separate `yaml` function for handling inline YAML strings?
6. **Parameter Validation**: How strict should we be with parameter type checking?

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

## Technical Notes

- The YAMLReader class handles reading YAML files and converting them to DuckDB tables
- The YAMLReadBind function sets up the output schema based on the YAML structure
- The YAMLReadFunction processes YAML nodes and fills output chunks
- Type detection is based on the YAML node type and content
- We use vectors of YAML nodes to support multi-document YAML files
- Reading top-level sequences treats each sequence item as a separate row
- Error recovery now properly handles partially invalid documents
- File handling supports both individual files and lists of files
- Current parameter validation may be too permissive in some cases

## Reminders for Conversation Continuity

- Review the TODO.md file to see what features are still to be implemented
- Check the test files to understand what functionality is already tested
- Pay attention to the build output for any compilation or runtime errors
- When adding new features, update the documentation accordingly
- Parameter validation and error handling need further improvements

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

## Update Log

- Initial version: Created during first conversation about simplifying the implementation
- Update 1: Added observations about the prompter, expanded technical notes about yaml-cpp integration, and added more detail on design decisions
- Update 2: Updated based on implementation progress - added information about test file organization, DuckDB-specific syntax for structs and lists, handling of top-level sequences, and added potential future features
- Update 3: Updated after implementing robust parameter handling and error handling - marked completed tasks, updated current status, and refined next steps
- Update 4: Updated with findings from our implementation of file globbing and file list support, improved error recovery, and modernizing the code with C++ best practices
- Update 5: Added testing findings regarding parameter validation, error messages, duplicate parameters, and idiomatic C++ usage. Updated with planned improvements that have been documented in TODO.md for future implementation
