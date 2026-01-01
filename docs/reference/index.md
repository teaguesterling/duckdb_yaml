# API Reference

Complete reference for all YAML extension functions, parameters, and capabilities.

## Overview

The YAML extension provides:

- **4 Table Functions** for reading and parsing YAML
- **12+ Scalar Functions** for manipulating YAML data
- **1 Copy Function** for writing YAML files
- **1 Custom Type** (YAML)

## Quick Navigation

| Section | Description |
|---------|-------------|
| [Table Functions](table-functions.md) | Functions that return tables |
| [Scalar Functions](scalar-functions.md) | Functions that return single values |
| [Parameters](parameters.md) | All configurable parameters |

## Function Summary

### Table Functions

| Function | Description |
|----------|-------------|
| `read_yaml(path, ...)` | Read YAML files into rows |
| `read_yaml_objects(path, ...)` | Read files preserving structure |
| `read_yaml_frontmatter(path, ...)` | Extract YAML frontmatter |
| `parse_yaml(string, ...)` | Parse YAML string into rows |
| `yaml_each(yaml)` | Iterate object key-value pairs |
| `yaml_array_elements(yaml)` | Unnest array elements |

### Scalar Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `yaml_extract(yaml, path)` | YAML | Extract value at path |
| `yaml_extract_string(yaml, path)` | VARCHAR | Extract as string |
| `yaml_exists(yaml, path)` | BOOLEAN | Check path existence |
| `yaml_type(yaml [, path])` | VARCHAR | Get value type |
| `yaml_keys(yaml [, path])` | VARCHAR[] | Get object keys |
| `yaml_array_length(yaml [, path])` | BIGINT | Get array length |
| `yaml_to_json(yaml)` | JSON | Convert to JSON |
| `value_to_yaml(any)` | YAML | Convert to YAML |
| `format_yaml(yaml, style)` | VARCHAR | Format with style |
| `yaml_valid(string)` | BOOLEAN | Validate YAML |
| `yaml_build_object(...)` | YAML | Build from pairs |
| `from_yaml(string, struct)` | Struct | Parse to struct |

### Configuration Functions

| Function | Description |
|----------|-------------|
| `yaml_set_default_style(style)` | Set output style |
| `yaml_get_default_style()` | Get current style |

### Copy Format

```sql
COPY table TO 'file.yaml' (FORMAT yaml [, STYLE style] [, LAYOUT layout])
```

## Type System

### YAML Type

The `YAML` type is a specialized VARCHAR:

```sql
-- Create YAML values
SELECT 'name: John'::YAML;
SELECT value_to_yaml({'key': 'value'});

-- Use in columns
CREATE TABLE t (data YAML);
```

### Type Conversions

| From | To | Method |
|------|-----|--------|
| YAML | JSON | `yaml_to_json(yaml)` or `yaml::JSON` |
| JSON | YAML | `json::YAML` |
| YAML | VARCHAR | `yaml::VARCHAR` |
| VARCHAR | YAML | `varchar::YAML` |
| Any | YAML | `value_to_yaml(value)` |

## Path Expressions

Used by extraction functions:

| Syntax | Meaning |
|--------|---------|
| `$` | Root |
| `$.key` | Object property |
| `$[n]` | Array index |
| `$.a.b.c` | Nested path |
| `$[0].key` | Mixed access |

## Error Handling

Most functions return `NULL` on error. Use `ignore_errors` parameter in read functions to continue on errors.

## Performance Notes

- YAML is stored as text; parsing occurs on function calls
- Use `columns` parameter for schema consistency
- Increase `sample_size` for complex schemas
- Consider JSON conversion for complex queries
