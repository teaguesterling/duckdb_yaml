# Utility Functions

Helper functions for validation and configuration.

## yaml_valid

Checks if a string is valid YAML.

### Syntax

```sql
yaml_valid(yaml_string) → BOOLEAN
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_string` | VARCHAR | The string to validate |

### Returns

BOOLEAN - `true` if valid YAML, `false` otherwise. Returns `NULL` for NULL input.

### Examples

```sql
-- Valid YAML
SELECT yaml_valid('name: John');
-- Returns: true

SELECT yaml_valid('items:\n  - one\n  - two');
-- Returns: true

-- Invalid YAML
SELECT yaml_valid('name: : invalid: syntax');
-- Returns: false

SELECT yaml_valid('unbalanced: [brackets');
-- Returns: false

-- NULL handling
SELECT yaml_valid(NULL);
-- Returns: NULL

-- Empty string
SELECT yaml_valid('');
-- Returns: true (empty document is valid)
```

### Use Cases

#### Filter Valid Records

```sql
SELECT *
FROM raw_yaml_data
WHERE yaml_valid(yaml_column);
```

#### Find Invalid Records

```sql
SELECT id, yaml_column
FROM raw_yaml_data
WHERE NOT yaml_valid(yaml_column);
```

#### Conditional Processing

```sql
SELECT
    id,
    CASE
        WHEN yaml_valid(yaml_col)
        THEN yaml_extract_string(yaml_col::YAML, '$.name')
        ELSE 'INVALID YAML'
    END AS name
FROM data;
```

#### Validation Report

```sql
SELECT
    COUNT(*) AS total_records,
    SUM(CASE WHEN yaml_valid(yaml_col) THEN 1 ELSE 0 END) AS valid_count,
    SUM(CASE WHEN NOT yaml_valid(yaml_col) THEN 1 ELSE 0 END) AS invalid_count
FROM raw_data;
```

---

## yaml_set_default_style

Sets the default output style for YAML formatting.

### Syntax

```sql
SELECT yaml_set_default_style(style);
```

### Parameters

| Parameter | Type | Values | Description |
|-----------|------|--------|-------------|
| `style` | VARCHAR | `'flow'`, `'block'` | The default style to use |

### Returns

VARCHAR - Confirmation message.

### Styles

| Style | Description | Example |
|-------|-------------|---------|
| `flow` | Compact, inline format | `{name: John, age: 30}` |
| `block` | Multi-line with indentation | `name: John`<br>`age: 30` |

### Examples

```sql
-- Set block style (more readable)
SELECT yaml_set_default_style('block');

-- Now value_to_yaml uses block style
SELECT value_to_yaml({'name': 'John', 'items': [1, 2, 3]});
-- Returns:
-- name: John
-- items:
--   - 1
--   - 2
--   - 3

-- Switch to flow style (compact)
SELECT yaml_set_default_style('flow');

SELECT value_to_yaml({'name': 'John', 'items': [1, 2, 3]});
-- Returns: {name: John, items: [1, 2, 3]}
```

### Session Scope

The setting applies to the current session:

```sql
-- Session 1
SELECT yaml_set_default_style('block');
-- Uses block style

-- Session 2 (separate connection)
-- Uses default flow style (independent of Session 1)
```

---

## yaml_get_default_style

Returns the current default YAML output style.

### Syntax

```sql
SELECT yaml_get_default_style();
```

### Returns

VARCHAR - The current default style (`'flow'` or `'block'`).

### Examples

```sql
-- Check current setting
SELECT yaml_get_default_style();
-- Returns: 'flow' (or 'block')

-- Use in conditional logic
SELECT
    CASE yaml_get_default_style()
        WHEN 'flow' THEN 'Compact mode'
        WHEN 'block' THEN 'Readable mode'
    END AS current_mode;
```

---

## Common Utility Patterns

### Validation Pipeline

```sql
-- Create validated table
CREATE TABLE validated_yaml AS
SELECT
    id,
    yaml_column,
    yaml_valid(yaml_column) AS is_valid,
    CASE
        WHEN yaml_valid(yaml_column)
        THEN yaml_type(yaml_column::YAML)
        ELSE NULL
    END AS root_type
FROM raw_input;

-- Process only valid records
INSERT INTO processed_data
SELECT id, yaml_column::YAML AS data
FROM validated_yaml
WHERE is_valid;
```

### Style Switching for Output

```sql
-- Save current style
CREATE TEMP TABLE style_backup AS
SELECT yaml_get_default_style() AS original_style;

-- Switch to block for readable output
SELECT yaml_set_default_style('block');

-- Generate readable YAML
COPY (SELECT value_to_yaml(config) FROM configs)
TO 'output.yaml';

-- Restore original style
SELECT yaml_set_default_style(
    (SELECT original_style FROM style_backup)
);
```

### Debug/Diagnostic Queries

```sql
-- YAML structure analysis
SELECT
    id,
    yaml_valid(data) AS valid,
    yaml_type(data::YAML) AS type,
    CASE yaml_type(data::YAML)
        WHEN 'object' THEN array_length(yaml_keys(data::YAML))
        WHEN 'array' THEN yaml_array_length(data::YAML)
        ELSE NULL
    END AS element_count
FROM yaml_data;
```

---

## Error Messages

The `yaml_valid` function does not provide detailed error messages. For debugging invalid YAML, try parsing and catch the error:

```sql
-- This will show the parse error
SELECT * FROM parse_yaml('invalid: : syntax');
-- Error: YAML parsing error at line 1, column 10: ...
```

## See Also

- [Error Handling Guide](../guide/error-handling.md) - Handling YAML errors
- [Conversion Functions](conversion.md) - Output formatting
- [YAML Type Guide](../guide/yaml-type.md) - YAML type overview
