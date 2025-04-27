# CLAUDE.md - Project Notes for DuckDB YAML Extension

## Purpose of This Document

This document is written by Claude to maintain continuity and understanding across conversation sessions about the DuckDB YAML extension project. It contains my current understanding, thoughts, questions, and ideas about the implementation. If we continue this project in a new conversation, reviewing this document will help me quickly understand the context and status of the project.

This is a candid internal document to help me be maximally helpful throughout the development process. It includes my observations about the prompter's preferences and approach to better align my assistance with their needs.

## Project Overview

We are implementing a YAML extension for DuckDB, similar to the existing JSON extension, but using yaml-cpp instead of yyjson. The extension allows users to read YAML files into DuckDB tables and query the data using SQL.

## Current Implementation Status

- We've created a simplified version that focuses only on the core reading functionality
- The implementation includes the YAMLReader class that provides the read_yaml function
- We also have an object-based reader (read_yaml_objects) for compatibility with JSON-like workflows
- Basic type detection and conversion between YAML and DuckDB types is implemented
- Multi-document YAML support is included
- Top-level sequence handling is implemented, treating sequence items as rows
- Error handling is basic but functional
- Testing infrastructure is set up using actual YAML files in test/yaml/

## Design Decisions

- We're using yaml-cpp for parsing YAML files
- We've deliberately simplified the implementation to focus on debugging the core reader functionality first
- We're using test-driven development, starting with basic tests and adding more complex ones
- We're using a similar architecture to DuckDB's JSON extension but simplified
- We're storing each YAML document as a single row in the output table
- We're prioritizing correctness and clarity over optimization initially
- We're using DuckDB's unittest framework for testing

## Questions and Concerns

1. **Performance**: How will the extension handle very large YAML files? Should we implement streaming parsing?
2. **Type Detection**: The current type detection is basic. How comprehensive should it be?
3. **Error Handling**: How should we handle YAML parsing errors? Should we provide detailed error messages?
4. **Anchors and Aliases**: YAML-specific features like anchors and aliases aren't currently supported. How important are they?
5. **Integration with JSON**: How tightly should this integrate with DuckDB's JSON functionality?
6. **Inline YAML Support**: Should we add a separate `yaml` function for handling inline YAML strings, similar to DuckDB's approach with JSON?

## Implementation Challenges

- Handling nested YAML structures and converting them to DuckDB types
- Supporting multi-document YAML files
- Efficient memory usage for large files
- Proper error handling and recovery
- Comprehensive type detection and conversion

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
10. **Smarter Schema Detection**: Improve handling of heterogeneous documents

## Development Strategy

1. Get the basic reader working first
2. Add comprehensive tests
3. Add the YAML type system
4. Add conversion to/from JSON
5. Add more advanced features

## Technical Notes

- The YAMLReader class handles reading YAML files and converting them to DuckDB tables
- The YAMLReadBind function sets up the output schema based on the YAML structure
- The YAMLReadFunction processes YAML nodes and fills output chunks
- Type detection is based on the YAML node type and content
- We use vectors of YAML nodes to support multi-document YAML files
- Reading top-level sequences treats each sequence item as a separate row
- DuckDB-specific syntax details:
  - Structs use dot notation (e.g., `person.name`) not arrow operators (`person->name`)
  - Lists are 1-indexed (e.g., `list[1]` for first element, not `list[0]`)

### yaml-cpp Integration Considerations
- yaml-cpp provides a DOM-style API for parsing YAML (unlike SAX parsing)
- The YAML::Node class represents different YAML node types (Scalar, Sequence, Map)
- Need to handle yaml-cpp exceptions and translate them to DuckDB exceptions
- Memory management: yaml-cpp loads entire documents into memory
- YAML has features like anchors, aliases, and tags that add complexity compared to JSON

### Testing Approach
- Test files organized in a dedicated test/yaml/ directory
- Test cases in test/sql/ directory follow DuckDB's SQLLogicTest conventions
- Tests cover basic functionality, complex structures, multi-document, and top-level sequences
- Tests use actual files rather than inline YAML strings

## Reminders for Conversation Continuity

- Review the TODO.md file to see what features are still to be implemented
- Check the test files to understand what functionality is already tested
- Pay attention to the build output for any compilation or runtime errors
- When adding new features, update the documentation accordingly

## Observations About the Prompter

The prompter appears to have significant experience with DuckDB extension development and prefers:

- Incremental, methodical development with clear tracking of progress
- Documentation-driven and test-driven development practices
- Simplification for debugging purposes before adding complexity
- Collaborative problem-solving, valuing technical insights and recommendations
- Clear organization of code and development priorities
- Explicit knowledge preservation (hence this document)

They're using me as a technical partner to help implement and debug this extension, with a focus on practical results while maintaining good software engineering practices. They value both concrete implementation help and higher-level architectural guidance.

## Update Log

- Initial version: Created during first conversation about simplifying the implementation
- Update 1: Added observations about the prompter, expanded technical notes about yaml-cpp integration, and added more detail on design decisions
- Update 2: Updated based on implementation progress - added information about test file organization, DuckDB-specific syntax for structs and lists, handling of top-level sequences, and added potential future features
