# Functions Reference

The YAML extension provides a comprehensive set of functions for working with YAML data.

## Function Categories

<div class="grid cards" markdown>

-   :material-magnify:{ .lg .middle } __Extraction Functions__

    ---

    Query and navigate YAML structures

    - `yaml_extract` - Extract value at path
    - `yaml_extract_string` - Extract as string
    - `yaml_exists` - Check path existence
    - `yaml_type` - Get value type
    - `yaml_keys` - Get object keys
    - `yaml_array_length` - Get array length

    [:octicons-arrow-right-24: Extraction Functions](extraction.md)

-   :material-swap-horizontal:{ .lg .middle } __Conversion Functions__

    ---

    Convert between YAML and other types

    - `yaml_to_json` - YAML to JSON
    - `value_to_yaml` - DuckDB value to YAML
    - `format_yaml` - Format with style
    - Type casts (YAML/JSON/VARCHAR)

    [:octicons-arrow-right-24: Conversion Functions](conversion.md)

-   :material-file-document:{ .lg .middle } __Parsing Functions__

    ---

    Parse YAML strings into data

    - `parse_yaml` - Parse to table
    - `from_yaml` - Parse to struct

    [:octicons-arrow-right-24: Parsing Functions](parsing.md)

-   :material-hammer-wrench:{ .lg .middle } __Construction Functions__

    ---

    Build YAML from components

    - `yaml_build_object` - Build from key-value pairs

    [:octicons-arrow-right-24: Construction Functions](construction.md)

-   :material-cog:{ .lg .middle } __Utility Functions__

    ---

    Helpers and validation

    - `yaml_valid` - Validate YAML
    - `yaml_set_default_style` - Set output style
    - `yaml_get_default_style` - Get current style

    [:octicons-arrow-right-24: Utility Functions](utility.md)

</div>

## Quick Reference

### Scalar Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `yaml_extract(yaml, path)` | YAML | Extract value at path |
| `yaml_extract_string(yaml, path)` | VARCHAR | Extract as string |
| `yaml_exists(yaml, path)` | BOOLEAN | Check if path exists |
| `yaml_type(yaml [, path])` | VARCHAR | Get value type |
| `yaml_keys(yaml [, path])` | VARCHAR[] | Get object keys |
| `yaml_array_length(yaml [, path])` | BIGINT | Get array length |
| `yaml_to_json(yaml)` | JSON | Convert to JSON |
| `value_to_yaml(any)` | YAML | Convert value to YAML |
| `format_yaml(yaml, style)` | VARCHAR | Format with style |
| `yaml_valid(varchar)` | BOOLEAN | Check if valid YAML |
| `from_yaml(str, struct)` | Struct | Parse to structured type |
| `yaml_build_object(...)` | YAML | Build from key-value pairs |

### Table Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `read_yaml(path, ...)` | TABLE | Read YAML files |
| `read_yaml_objects(path, ...)` | TABLE | Read preserving structure |
| `read_yaml_frontmatter(path, ...)` | TABLE | Extract frontmatter |
| `parse_yaml(string, ...)` | TABLE | Parse YAML string |
| `yaml_each(yaml)` | TABLE(key, value) | Iterate key-value pairs |
| `yaml_array_elements(yaml)` | TABLE(value) | Unnest array elements |

### Configuration Functions

| Function | Effect |
|----------|--------|
| `yaml_set_default_style('block'\|'flow')` | Set default output style |
| `yaml_get_default_style()` | Get current default style |

## Path Expression Syntax

Many functions accept a `path` parameter using JSONPath-like syntax:

| Syntax | Description | Example |
|--------|-------------|---------|
| `$` | Root of document | `$` |
| `$.key` | Object property | `$.name` |
| `$[n]` | Array element by index | `$[0]` |
| `$.a.b.c` | Nested property | `$.user.address.city` |
| `$[0].key` | Array element property | `$[0].name` |

Examples:

```sql
-- Root value
yaml_extract(data, '$')

-- Top-level property
yaml_extract(data, '$.name')

-- Nested property
yaml_extract(data, '$.user.address.city')

-- Array element
yaml_extract(data, '$.items[0]')

-- Nested in array
yaml_extract(data, '$.items[0].price')
```

## Type Return Values

The `yaml_type` function returns one of:

| Return Value | Description |
|--------------|-------------|
| `'object'` | YAML mapping/object |
| `'array'` | YAML sequence/array |
| `'scalar'` | Scalar value (string, number, etc.) |
| `'null'` | Null value |

## Error Handling

Most functions return NULL for:

- Invalid paths (path doesn't exist)
- Type mismatches (accessing array index on object)
- NULL input

Use `yaml_exists` to check before extraction:

```sql
SELECT
    CASE
        WHEN yaml_exists(data, '$.optional')
        THEN yaml_extract_string(data, '$.optional')
        ELSE 'default'
    END
FROM yaml_table;
```
