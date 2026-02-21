# Scalar Functions Reference

Complete reference for YAML scalar functions.

## Extraction Functions

### yaml_extract

```sql
yaml_extract(yaml YAML, path VARCHAR) → YAML
```

Extracts a value at the specified path.

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml` | YAML | Source YAML value |
| `path` | VARCHAR | JSONPath-like expression |

**Returns:** YAML value at path, or NULL if not found.

```sql
SELECT yaml_extract('{user: {name: John}}'::YAML, '$.user.name');
-- Returns: John
```

---

### yaml_extract_string

```sql
yaml_extract_string(yaml YAML, path VARCHAR) → VARCHAR
```

Extracts a value as a string.

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml` | YAML | Source YAML value |
| `path` | VARCHAR | JSONPath-like expression |

**Returns:** VARCHAR representation of value, or NULL.

```sql
SELECT yaml_extract_string('{count: 42}'::YAML, '$.count');
-- Returns: '42'
```

---

### yaml_exists

```sql
yaml_exists(yaml YAML, path VARCHAR) → BOOLEAN
```

Checks if a path exists.

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml` | YAML | Source YAML value |
| `path` | VARCHAR | JSONPath-like expression |

**Returns:** TRUE if path exists, FALSE otherwise.

```sql
SELECT yaml_exists('{name: John}'::YAML, '$.age');
-- Returns: false
```

---

### yaml_type

```sql
yaml_type(yaml YAML) → VARCHAR
yaml_type(yaml YAML, path VARCHAR) → VARCHAR
```

Returns the type of a YAML value.

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml` | YAML | Source YAML value |
| `path` | VARCHAR | Optional path expression |

**Returns:** One of: `'object'`, `'array'`, `'scalar'`, `'null'`

```sql
SELECT yaml_type('[1, 2, 3]'::YAML);
-- Returns: 'array'
```

---

### yaml_keys

```sql
yaml_keys(yaml YAML) → VARCHAR[]
yaml_keys(yaml YAML, path VARCHAR) → VARCHAR[]
```

Returns keys of a YAML object.

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml` | YAML | Source YAML value |
| `path` | VARCHAR | Optional path to object |

**Returns:** Array of key names, or NULL if not an object.

```sql
SELECT yaml_keys('{a: 1, b: 2, c: 3}'::YAML);
-- Returns: [a, b, c]
```

---

### yaml_array_length

```sql
yaml_array_length(yaml YAML) → BIGINT
yaml_array_length(yaml YAML, path VARCHAR) → BIGINT
```

Returns the length of a YAML array.

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml` | YAML | Source YAML value |
| `path` | VARCHAR | Optional path to array |

**Returns:** Number of elements, or NULL if not an array.

```sql
SELECT yaml_array_length('[1, 2, 3, 4, 5]'::YAML);
-- Returns: 5
```

---

## Conversion Functions

### yaml_to_json

```sql
yaml_to_json(yaml YAML) → JSON
```

Converts YAML to JSON.

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml` | YAML | YAML value to convert |

**Returns:** JSON equivalent.

```sql
SELECT yaml_to_json('name: John\nage: 30'::YAML);
-- Returns: {"name":"John","age":30}
```

---

### value_to_yaml

```sql
value_to_yaml(value ANY) → YAML
```

Converts any DuckDB value to YAML.

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | ANY | Value to convert |

**Returns:** YAML representation.

```sql
SELECT value_to_yaml({'name': 'John', 'scores': [1, 2, 3]});
-- Returns: name: John\nscores:\n  - 1\n  - 2\n  - 3
```

---

### format_yaml

```sql
format_yaml(value ANY, style := 'flow', multiline := 'auto', indent := 2) → VARCHAR
```

Formats a value as YAML with configurable style.

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | ANY | Value to format as YAML |
| `style` | VARCHAR | `'flow'` or `'block'` (named parameter) |
| `multiline` | VARCHAR | `'auto'`, `'literal'`, or `'quoted'` (named parameter) |
| `indent` | INTEGER | `1`-`10`, indentation width (named parameter) |

**Returns:** Formatted YAML string.

```sql
SELECT format_yaml({'a': 1, 'b': 2}, style := 'block');
-- Returns:
-- a: 1
-- b: 2

-- Multiline strings with literal block scalars
SELECT format_yaml({'msg': E'hello\nworld'}, style := 'block', multiline := 'literal');
-- Returns:
-- msg: |
--   hello
--   world

-- Custom indentation
SELECT format_yaml({'a': {'b': 1}}, style := 'block', indent := 4);
-- Returns:
-- a:
--     b: 1
```

---

### from_yaml

```sql
from_yaml(yaml_string VARCHAR, structure STRUCT) → STRUCT
```

Parses YAML into a structured type.

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_string` | VARCHAR | YAML string to parse |
| `structure` | STRUCT | Template defining output structure |

**Returns:** Struct matching the template.

```sql
SELECT from_yaml('name: John\nage: 30', {'name': '', 'age': 0});
-- Returns: {'name': John, 'age': 30}
```

---

## Construction Functions

### yaml_build_object

```sql
yaml_build_object(key1, value1, key2, value2, ...) → YAML
```

Builds a YAML object from key-value pairs.

| Parameter | Type | Description |
|-----------|------|-------------|
| `key` | VARCHAR | Object key |
| `value` | ANY | Value for key |

**Returns:** YAML object.

```sql
SELECT yaml_build_object('name', 'John', 'age', 30);
-- Returns: {name: John, age: 30}
```

---

## Utility Functions

### yaml_valid

```sql
yaml_valid(yaml_string VARCHAR) → BOOLEAN
```

Checks if a string is valid YAML.

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_string` | VARCHAR | String to validate |

**Returns:** TRUE if valid, FALSE otherwise.

```sql
SELECT yaml_valid('name: : invalid');
-- Returns: false
```

---

### yaml_set_default_style

```sql
yaml_set_default_style(style VARCHAR) → VARCHAR
```

Sets the default output style.

| Parameter | Type | Description |
|-----------|------|-------------|
| `style` | VARCHAR | `'flow'` or `'block'` |

**Returns:** Confirmation message.

```sql
SELECT yaml_set_default_style('block');
```

---

### yaml_get_default_style

```sql
yaml_get_default_style() → VARCHAR
```

Returns the current default style.

**Returns:** `'flow'` or `'block'`.

```sql
SELECT yaml_get_default_style();
-- Returns: flow
```

---

## Type Casts

### YAML to JSON

```sql
yaml_value::JSON
CAST(yaml_value AS JSON)
```

### JSON to YAML

```sql
json_value::YAML
CAST(json_value AS YAML)
```

### VARCHAR to YAML

```sql
string_value::YAML
CAST(string_value AS YAML)
```

### YAML to VARCHAR

```sql
yaml_value::VARCHAR
CAST(yaml_value AS VARCHAR)
```
