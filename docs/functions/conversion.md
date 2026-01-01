# Conversion Functions

Functions for converting between YAML and other data types.

## yaml_to_json

Converts a YAML value to JSON.

### Syntax

```sql
yaml_to_json(yaml_value) → JSON
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_value` | YAML | The YAML value to convert |

### Returns

JSON - The equivalent JSON value.

### Examples

```sql
-- Simple object
SELECT yaml_to_json('name: John\nage: 30'::YAML);
-- Returns: {"name":"John","age":30}

-- Array
SELECT yaml_to_json('[1, 2, 3]'::YAML);
-- Returns: [1,2,3]

-- Nested structure
SELECT yaml_to_json('
user:
  name: John
  scores: [85, 90, 95]
'::YAML);
-- Returns: {"user":{"name":"John","scores":[85,90,95]}}

-- Multi-document YAML becomes JSON array
SELECT yaml_to_json('---
name: Doc1
---
name: Doc2'::YAML);
-- Returns: [{"name":"Doc1"},{"name":"Doc2"}]
```

### Notes

- YAML-specific features (anchors, aliases, tags) are resolved before conversion
- Multi-document YAML is converted to a JSON array
- YAML comments are not preserved

---

## value_to_yaml

Converts a DuckDB value to YAML format.

### Syntax

```sql
value_to_yaml(value) → YAML
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | ANY | Any DuckDB value |

### Returns

YAML - The value formatted as YAML.

### Examples

```sql
-- Struct to YAML
SELECT value_to_yaml({'name': 'John', 'age': 30});
-- Returns:
-- name: John
-- age: 30

-- List to YAML
SELECT value_to_yaml([1, 2, 3, 4, 5]);
-- Returns:
-- - 1
-- - 2
-- - 3
-- - 4
-- - 5

-- Nested structure
SELECT value_to_yaml({
    'user': {'name': 'John', 'email': 'john@example.com'},
    'roles': ['admin', 'user']
});
-- Returns:
-- user:
--   name: John
--   email: john@example.com
-- roles:
--   - admin
--   - user

-- NULL value
SELECT value_to_yaml(NULL);
-- Returns: null

-- Boolean values
SELECT value_to_yaml(true);
-- Returns: true
```

### Supported Types

| DuckDB Type | YAML Output |
|-------------|-------------|
| VARCHAR | String (quoted if needed) |
| INTEGER, BIGINT | Integer |
| DOUBLE, FLOAT | Float |
| BOOLEAN | `true` / `false` |
| DATE | ISO date string |
| TIMESTAMP | ISO timestamp string |
| STRUCT | Mapping |
| LIST | Sequence |
| MAP | Mapping |
| NULL | `null` |

---

## format_yaml

Formats YAML with a specific style.

### Syntax

```sql
format_yaml(yaml_value, style) → VARCHAR
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_value` | YAML | The YAML value to format |
| `style` | VARCHAR | Either `'flow'` or `'block'` |

### Returns

VARCHAR - The formatted YAML string.

### Examples

```sql
-- Flow style (compact)
SELECT format_yaml('{name: John, items: [1, 2, 3]}'::YAML, 'flow');
-- Returns: {name: John, items: [1, 2, 3]}

-- Block style (readable)
SELECT format_yaml('{name: John, items: [1, 2, 3]}'::YAML, 'block');
-- Returns:
-- name: John
-- items:
--   - 1
--   - 2
--   - 3
```

### Styles

| Style | Description | Use Case |
|-------|-------------|----------|
| `flow` | Compact, inline format | Storage, data transfer |
| `block` | Multi-line with indentation | Configuration files, readability |

---

## Type Casts

### YAML to JSON Cast

```sql
SELECT yaml_value::JSON FROM table;
SELECT CAST('name: John' AS YAML)::JSON;
```

### JSON to YAML Cast

```sql
SELECT json_value::YAML FROM table;
SELECT '{"name":"John"}'::JSON::YAML;
```

### YAML to VARCHAR Cast

```sql
SELECT yaml_value::VARCHAR FROM table;
```

### VARCHAR to YAML Cast

```sql
SELECT 'name: John'::YAML;
SELECT varchar_column::YAML FROM table;
```

### Examples

```sql
-- Chain conversions
SELECT '{"a":1}'::JSON::YAML::VARCHAR;
-- Returns: {a: 1}

-- In expressions
SELECT
    CASE
        WHEN format = 'json' THEN yaml_data::JSON::VARCHAR
        ELSE yaml_data::VARCHAR
    END AS output
FROM configs;
```

---

## yaml_set_default_style

Sets the default YAML output style for the session.

### Syntax

```sql
SELECT yaml_set_default_style(style);
```

### Parameters

| Parameter | Type | Values |
|-----------|------|--------|
| `style` | VARCHAR | `'flow'` or `'block'` |

### Examples

```sql
-- Set block style as default
SELECT yaml_set_default_style('block');

-- Now value_to_yaml uses block style
SELECT value_to_yaml({'name': 'John'});
-- Returns:
-- name: John

-- Switch to flow style
SELECT yaml_set_default_style('flow');

SELECT value_to_yaml({'name': 'John'});
-- Returns: {name: John}
```

---

## yaml_get_default_style

Gets the current default YAML output style.

### Syntax

```sql
SELECT yaml_get_default_style();
```

### Returns

VARCHAR - The current default style (`'flow'` or `'block'`).

### Examples

```sql
SELECT yaml_get_default_style();
-- Returns: flow (or block, depending on setting)
```

---

## Conversion Patterns

### Round-Trip Conversion

```sql
-- YAML → JSON → YAML (may change formatting)
SELECT '{"a":1}'::JSON::YAML::JSON;

-- Preserves data, may change representation
SELECT yaml_to_json(value_to_yaml({'a': 1}));
```

### Database Integration

```sql
-- Store YAML, query as JSON
CREATE TABLE configs (id INT, data YAML);

INSERT INTO configs VALUES (1, 'name: app\nversion: 1.0');

SELECT id, data::JSON AS json_data FROM configs;
```

### Export Transformation

```sql
-- Transform and export
COPY (
    SELECT
        id,
        yaml_to_json(yaml_data) AS json_data
    FROM source_table
) TO 'output.json';
```

## See Also

- [Extraction Functions](extraction.md) - Querying YAML
- [Parsing Functions](parsing.md) - Parsing YAML strings
- [YAML Type Guide](../guide/yaml-type.md) - YAML type overview
