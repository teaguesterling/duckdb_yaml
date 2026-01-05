Release Notes: v1.5.0

## New Extraction Functions for JSON Parity

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

## Function Aliases

Added aliases for consistency with DuckDB's JSON functions ([#29](https://github.com/teaguesterling/duckdb_yaml/issues/29), [#31](https://github.com/teaguesterling/duckdb_yaml/issues/31)):

| Alias | Equivalent Function |
|-------|---------------------|
| `yaml_extract_path` | `yaml_extract` |
| `yaml_extract_path_text` | `yaml_extract_string` |
| `to_yaml` | `value_to_yaml` |

## Documentation

- Updated extraction function documentation with new functions
- Added function aliases reference table
