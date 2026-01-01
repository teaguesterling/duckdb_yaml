# User Guide

This guide covers the core features of the DuckDB YAML extension in depth.

## Overview

The YAML extension provides comprehensive support for working with YAML data in DuckDB:

| Feature | Description |
|---------|-------------|
| [Reading YAML Files](reading-yaml.md) | Load YAML files into tables with automatic schema detection |
| [Frontmatter Extraction](frontmatter.md) | Extract YAML frontmatter from Markdown and text files |
| [YAML Type](yaml-type.md) | Native YAML type with conversion functions |
| [Writing YAML Files](writing-yaml.md) | Export query results to YAML format |
| [Type Detection](type-detection.md) | Automatic detection of dates, times, and numeric types |
| [Error Handling](error-handling.md) | Robust error recovery and validation |

## Core Concepts

### YAML Documents

YAML supports multiple documents in a single file, separated by `---`:

```yaml
---
name: Document 1
---
name: Document 2
```

The extension handles multi-document YAML automatically, treating each document as a separate row.

### YAML Sequences

Top-level sequences are expanded into individual rows:

```yaml
- name: Alice
  age: 30
- name: Bob
  age: 25
```

This produces two rows when read with `read_yaml`.

### Path Expressions

Many functions use path expressions to navigate YAML structures:

- `$` - Root of the document
- `$.field` - Access object field
- `$[0]` - Access array element
- `$.user.address.city` - Nested access

### Type System

The extension provides a native `YAML` type that:

- Stores YAML as text internally
- Supports casting to/from JSON and VARCHAR
- Enables type-safe operations

## Quick Reference

### Reading Functions

```sql
-- Table functions
read_yaml(path, ...)           -- Read YAML files into rows
read_yaml_objects(path, ...)   -- Read files preserving structure
read_yaml_frontmatter(path, ...)  -- Extract frontmatter from files
parse_yaml(string, ...)        -- Parse YAML string into rows
```

### Extraction Functions

```sql
yaml_extract(yaml, path)         -- Extract value at path
yaml_extract_string(yaml, path)  -- Extract as string
yaml_exists(yaml, path)          -- Check if path exists
yaml_type(yaml [, path])         -- Get value type
yaml_keys(yaml [, path])         -- Get object keys
yaml_array_length(yaml [, path]) -- Get array length
```

### Conversion Functions

```sql
yaml_to_json(yaml)    -- Convert to JSON
value_to_yaml(value)  -- Convert DuckDB value to YAML
format_yaml(yaml, style)  -- Format with specific style
```

### Table Functions

```sql
yaml_each(yaml)            -- Key-value pairs
yaml_array_elements(yaml)  -- Array elements as rows
```

## Next Steps

Start with [Reading YAML Files](reading-yaml.md) to learn how to load YAML data into DuckDB.
