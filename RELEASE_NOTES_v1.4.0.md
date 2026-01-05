Release Notes: v1.4.0

New Features

### New Extraction Functions for JSON Parity

Added several new functions to match DuckDB's JSON extension capabilities:

- **`->>` operator** ([#23](https://github.com/teaguesterling/duckdb_yaml/issues/23)): Arrow operator for string extraction
  ```sql
  SELECT '{name: John}'::YAML ->> '$.name';  -- Returns: 'John'
  ```

- **`yaml_structure`** ([#24](https://github.com/teaguesterling/duckdb_yaml/issues/24)): Returns JSON schema of YAML document
  ```sql
  SELECT yaml_structure('name: Alice\nage: 30');
  -- Returns: {"name":"VARCHAR","age":"UBIGINT"}
  ```

- **`yaml_contains`** ([#25](https://github.com/teaguesterling/duckdb_yaml/issues/25)): Check if YAML contains another
  ```sql
  SELECT yaml_contains('{a: 1, b: 2}', '{a: 1}');  -- Returns: true
  ```

- **`yaml_merge_patch`** ([#26](https://github.com/teaguesterling/duckdb_yaml/issues/26)): RFC 7386 merge patch
  ```sql
  SELECT yaml_merge_patch('{a: 1}', '{b: 2}');  -- Returns: {a: 1, b: 2}
  ```

- **`yaml_value`** ([#28](https://github.com/teaguesterling/duckdb_yaml/issues/28)): Extract scalar values only (NULL for arrays/objects)
  ```sql
  SELECT yaml_value('{count: 42}', '$.count');  -- Returns: '42'
  ```

### Function Aliases ([#29](https://github.com/teaguesterling/duckdb_yaml/issues/29), [#31](https://github.com/teaguesterling/duckdb_yaml/issues/31))

Added aliases for consistency with DuckDB's JSON functions:
- `yaml_extract_path` → `yaml_extract`
- `yaml_extract_path_text` → `yaml_extract_string`
- `to_yaml` → `value_to_yaml`

### records Parameter for Nested Data Extraction ([#22](https://github.com/teaguesterling/duckdb_yaml/issues/22))

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

