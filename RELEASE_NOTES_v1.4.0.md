Release Notes: v1.4.0

New Features

records Parameter for Nested Data Extraction (https://github.com/teaguesterling/duckdb_yaml/issues/22)

Extract records from nested arrays using dot-notation paths:
-- Given YAML with structure: {data: {items: [{id: 1}, {id: 2}]}}
SELECT * FROM read_yaml('nested.yaml', records := 'data.items');
- Dot-notation path navigation (e.g., records := 'data.items')
- Proper error messages for missing paths and non-sequence targets
- Integration with ignore_errors parameter for graceful handling

parse_yaml Table Function and from_yaml Scalar Function (https://github.com/teaguesterling/duckdb_yaml/issues/15, https://github.com/teaguesterling/duckdb_yaml/issues/16)

- parse_yaml: Table function that parses YAML strings into rows with multi-document and sequence expansion support
- from_yaml: Scalar function that converts YAML strings to typed structs

Sampling Parameters for Schema Detection

Control schema detection performance with large file sets:
- sample_size: Number of rows to sample (default: 20480, use -1 for unlimited)
- maximum_sample_files: Maximum files to sample (default: 32, use -1 for unlimited)

Bug Fixes

Schema Inference for Nested Arrays with Optional Fields (https://github.com/teaguesterling/duckdb_yaml/issues/21)

Schema inference now correctly detects the union of all fields across all elements in nested arrays:
# Before: items inferred as STRUCT(a)[] - field b was dropped
# After:  items inferred as STRUCT(a, b)[] - field b is NULL where missing
items:
  - {a: 1}
  - {a: 2, b: 3}

Type Conflict Handling (https://github.com/teaguesterling/duckdb_yaml/issues/17, https://github.com/teaguesterling/duckdb_yaml/issues/18, https://github.com/teaguesterling/duckdb_yaml/issues/19)

When schema detection encounters type conflicts (e.g., STRUCT vs scalar), the reader now falls back to the YAML type instead of VARCHAR, preserving all data.

Documentation

- Added comprehensive MkDocs documentation with Material theme (light/dark mode)
- Read the Docs integration at readthedocs.io
- User guides, API reference, and examples

Internal Changes

- CI cleanup and build improvements
- Platform-dependent test fixes
- Submodule updates

