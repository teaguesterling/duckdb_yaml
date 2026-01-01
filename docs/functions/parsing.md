# Parsing Functions

Functions for parsing YAML strings into structured data.

## parse_yaml

Table function that parses a YAML string into rows.

### Syntax

```sql
parse_yaml(yaml_string) → TABLE
parse_yaml(yaml_string, options...) → TABLE
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_string` | VARCHAR | The YAML string to parse |

### Returns

TABLE - Rows with columns detected from the YAML structure.

### Examples

#### Single Document

```sql
SELECT * FROM parse_yaml('name: John
age: 30
city: NYC');
```

Result:

| name | age | city |
|------|-----|------|
| John | 30 | NYC |

#### Multi-Document YAML

```sql
SELECT * FROM parse_yaml('---
name: John
role: admin
---
name: Jane
role: user');
```

Result:

| name | role |
|------|------|
| John | admin |
| Jane | user |

#### YAML Sequence

```sql
SELECT * FROM parse_yaml('- name: Alice
  score: 95
- name: Bob
  score: 87
- name: Carol
  score: 92');
```

Result:

| name | score |
|------|-------|
| Alice | 95 |
| Bob | 87 |
| Carol | 92 |

#### Nested Structures

```sql
SELECT * FROM parse_yaml('user:
  name: John
  email: john@example.com
settings:
  theme: dark
  notifications: true');
```

Result:

| user | settings |
|------|----------|
| {name: John, email: john@example.com} | {theme: dark, notifications: true} |

### Usage Patterns

```sql
-- Parse and query
SELECT name, score
FROM parse_yaml(yaml_content)
WHERE score > 90;

-- Parse from table column
SELECT t.id, p.*
FROM my_table t
CROSS JOIN LATERAL parse_yaml(t.yaml_column) p;

-- With explicit string
SELECT *
FROM parse_yaml($yaml$
items:
  - product: Widget
    price: 10.99
  - product: Gadget
    price: 24.99
$yaml$);
```

---

## from_yaml

Scalar function that parses a YAML string into a structured type.

### Syntax

```sql
from_yaml(yaml_string, structure) → Struct
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_string` | VARCHAR | The YAML string to parse |
| `structure` | Struct | Template defining the output structure |

### Returns

Struct - The parsed data matching the template structure.

### Examples

#### Basic Usage

```sql
SELECT from_yaml(
    'name: John
age: 30',
    {'name': '', 'age': 0}
);
-- Returns: {'name': John, 'age': 30}
```

#### Nested Structures

```sql
SELECT from_yaml(
    'user:
  name: John
  email: john@example.com',
    {'user': {'name': '', 'email': ''}}
);
-- Returns: {'user': {'name': John, 'email': john@example.com}}
```

#### With Arrays

```sql
SELECT from_yaml(
    'items:
  - Apple
  - Banana
  - Orange',
    {'items': ['']}
);
-- Returns: {'items': [Apple, Banana, Orange]}
```

#### In Queries

```sql
-- Extract structured data from YAML column
SELECT
    id,
    (from_yaml(yaml_data, {'name': '', 'version': ''})).name AS name,
    (from_yaml(yaml_data, {'name': '', 'version': ''})).version AS version
FROM packages;

-- With LATERAL join
SELECT t.id, parsed.*
FROM my_table t
CROSS JOIN LATERAL (
    SELECT from_yaml(t.yaml_col, {'key': '', 'value': 0}) AS data
) parsed;
```

### Type Inference

The structure parameter determines output types:

| Template Value | Inferred Type |
|---------------|---------------|
| `''` | VARCHAR |
| `0` | INTEGER |
| `0.0` | DOUBLE |
| `true` | BOOLEAN |
| `['']` | VARCHAR[] |
| `[0]` | INTEGER[] |
| `{'k': ''}` | STRUCT |

### Examples with Types

```sql
-- Integer values
SELECT from_yaml('count: 42', {'count': 0});
-- Returns: {'count': 42} (count is INTEGER)

-- Boolean values
SELECT from_yaml('active: true', {'active': true});
-- Returns: {'active': true} (active is BOOLEAN)

-- Mixed types
SELECT from_yaml(
    'name: Test
count: 5
ratio: 0.75
enabled: true',
    {'name': '', 'count': 0, 'ratio': 0.0, 'enabled': true}
);
```

---

## Parsing Patterns

### Handling Dynamic YAML

When structure varies, use flexible approaches:

```sql
-- Parse to YAML type first, then extract
SELECT
    yaml_extract_string(data::YAML, '$.name') AS name,
    yaml_extract(data::YAML, '$.config') AS config
FROM raw_data;

-- Or use parse_yaml for tabular data
SELECT * FROM parse_yaml(dynamic_yaml);
```

### Error Handling

```sql
-- from_yaml returns NULL for invalid YAML
SELECT from_yaml('invalid: : yaml', {'key': ''});
-- Returns: NULL

-- Use COALESCE for defaults
SELECT COALESCE(
    from_yaml(yaml_col, {'name': ''}),
    {'name': 'unknown'}
) AS parsed
FROM data;
```

### Combining with Other Functions

```sql
-- Parse and convert
SELECT yaml_to_json(
    value_to_yaml(
        from_yaml('name: John\nage: 30', {'name': '', 'age': 0})
    )
);

-- Parse and extract
SELECT yaml_extract(
    parse_yaml('data: {nested: value}').*::YAML,
    '$.data.nested'
);
```

## See Also

- [Extraction Functions](extraction.md) - Querying parsed YAML
- [Conversion Functions](conversion.md) - Type conversions
- [Reading YAML Files](../guide/reading-yaml.md) - File-based parsing
