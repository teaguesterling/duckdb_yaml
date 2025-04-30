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
- Direct file path support (SELECT * FROM 'file.yaml') is now implemented
- Test coverage for basic functionality, error handling, and direct file path usage

## Recent Changes and Findings

1. **Direct File Path Support**:
   - Implemented support for directly using YAML files in FROM clauses (e.g., `SELECT * FROM 'file.yaml'`)
   - Registered both `.yaml` and `.yml` file extensions with DuckDB's configuration system
   - Created a lambda handler that transforms file paths to `read_yaml` function calls
   - Added comprehensive tests for this functionality
   - Updated documentation to reflect this new capability

2. **Enhanced File Handling**:
   - Added support for providing a list of file paths (e.g., `read_yaml(['a.yaml', 'b.yaml'])`)
   - Implemented globbing using built-in `fs.Glob` functionality
   - Added support for both `.yaml` and `.yml` extensions

3. **Improved Error Recovery**:
   - Implemented a robust recovery mechanism for partially invalid YAML files
   - Enhanced the `RecoverPartialYAMLDocuments` function to handle document separators
   - Made error handling more granular (per-file and per-document)

4. **Code Modernization**:
   - Replaced C-style strings with modern C++ string objects
   - Added const qualifiers and references for better performance and safety
   - Made function signatures more consistent with DuckDB's style

5. **Integration with DuckDB's File System**:
   - The direct file path implementation reveals how deeply DuckDB integrates file handling into its SQL parsing system
   - We've leveraged DuckDB's existing infrastructure for file extension handling
   - This approach provides a more natural SQL experience for users

6. **Testing Findings**:
   - Parameter type checking is too permissive (e.g., `auto_detect='yes'` passes)
   - Expected error messages don't always match actual DuckDB errors
   - Duplicate parameter detection isn't working as expected
   - Some memory management could be improved with more idiomatic C++
   - DuckDB's file extension system currently doesn't support passing named parameters through the direct syntax
  
7. **API Compatibility Fix for Direct File Path Support**:
   - Initial implementation using TableFunctionRef and DBConfig::RegisterFileExtension encountered API compatibility issues
   - Switched to a simpler approach using FileSystem::RegisterSubstrait that directly maps file extensions to function names
   - Simplified test cases to focus on core functionality
   - This experience highlighted the importance of working within DuckDB's established patterns

## Design Decisions

- We're using yaml-cpp for parsing YAML files
- We're using a similar architecture to DuckDB's JSON extension but simplified
- We're storing each YAML document as a single row in the output table
- We're prioritizing correctness and clarity over optimization initially
- We're using DuckDB's unittest framework for testing
- We're providing similar file handling capabilities to the JSON extension
- We've decided to defer some parameter validation improvements for a future update
- For direct file path support, we're calling read_yaml with default parameters, requiring users to use the explicit function call for customization

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

### Core YAML Reading Functionality
- The YAMLReader class handles reading YAML files and converting them to DuckDB tables
- The YAMLReadBind function sets up the output schema based on the YAML structure
- The YAMLReadFunction processes YAML nodes and fills output chunks
- Type detection is based on the YAML node type and content
- We use vectors of YAML nodes to support multi-document YAML files
- Reading top-level sequences treats each sequence item as a separate row
- Error recovery now properly handles partially invalid documents
- File handling supports both individual files and lists of files

### Direct File Path Support
- Implementation leverages DuckDB's file extension registry system (`DBConfig::RegisterFileExtension`)
- We've registered handlers for both `.yaml` and `.yml` extensions
- The handler creates a `TableFunctionRef` AST node pointing to `read_yaml`
- The path is added as a `ConstantExpression` parameter to the function
- This approach is consistent with how other DuckDB extensions handle file paths
- The implementation supports all SQL capabilities like filtering, joining, and aggregation

### DuckDB Integration
- DuckDB has a sophisticated extension system that allows deep integration
- File extension handlers are called during SQL parsing, before execution
- We've observed that DuckDB's extension mechanisms are both powerful and well-designed
- Our implementation fits well within DuckDB's patterns while maintaining a clean interface

## Reminders for Conversation Continuity

- Review the TODO.md file to see what features are still to be implemented
- Check the test files to understand what functionality is already tested
- Pay attention to the build output for any compilation or runtime errors
- When adding new features, update the documentation accordingly
- Parameter validation and error handling need further improvements
- Remember that direct file path support uses default parameters only

### Direct File Path Support
- Implementation uses FileSystem::RegisterSubstrait to map file extensions to function names
- Both .yaml and .yml extensions are registered to use the read_yaml function
- This simple approach achieves the goal of allowing direct file references in FROM clauses
- The approach aligns with DuckDB's current API patterns for extension file handlers
- The implementation supports filtering, aggregation, and table creation on the file data

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
- Update 6: Added detailed information about direct file path support implementation, including technical details, integration with DuckDB's file extension system, and observations about limitations and future possibilities. Expanded observations about the prompter's preferences based on the request for this feature.
- Update 7: Fixed API compatibility issues with direct file path implementation. Replaced complex TableFunctionRef/DBConfig approach with simpler FileSystem::RegisterSubstrait method. Updated documentation and tests to reflect actual capabilities. Added notes about the importance of aligning with DuckDB's current API patterns.

