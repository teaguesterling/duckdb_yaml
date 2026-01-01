# Extraction Functions

Functions for querying and navigating YAML structures.

## yaml_extract

Extracts a value from a YAML document at the specified path.

### Syntax

```sql
yaml_extract(yaml_value, path) → YAML
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_value` | YAML | The YAML value to query |
| `path` | VARCHAR | JSONPath-like path expression |

### Returns

YAML - The extracted value, or NULL if path doesn't exist.

### Examples

```sql
-- Extract object property
SELECT yaml_extract('{name: John, age: 30}'::YAML, '$.name');
-- Returns: John

-- Extract nested property
SELECT yaml_extract('{user: {id: 123, email: test@example.com}}'::YAML, '$.user.email');
-- Returns: test@example.com

-- Extract array element
SELECT yaml_extract('[1, 2, 3, 4, 5]'::YAML, '$[2]');
-- Returns: 3

-- Extract nested array
SELECT yaml_extract('[1, 2, [3, 4, 5]]'::YAML, '$[2][1]');
-- Returns: 4

-- Extract from array of objects
SELECT yaml_extract('[{name: Alice}, {name: Bob}]'::YAML, '$[1].name');
-- Returns: Bob

-- Path not found returns NULL
SELECT yaml_extract('{name: John}'::YAML, '$.missing');
-- Returns: NULL
```

---

## yaml_extract_string

Extracts a value from a YAML document as a VARCHAR string.

### Syntax

```sql
yaml_extract_string(yaml_value, path) → VARCHAR
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_value` | YAML | The YAML value to query |
| `path` | VARCHAR | JSONPath-like path expression |

### Returns

VARCHAR - The extracted value as a string, or NULL if path doesn't exist.

### Examples

```sql
-- Extract string value
SELECT yaml_extract_string('{name: John}'::YAML, '$.name');
-- Returns: 'John'

-- Numbers are converted to strings
SELECT yaml_extract_string('{count: 42}'::YAML, '$.count');
-- Returns: '42'

-- Complex values are serialized
SELECT yaml_extract_string('{items: [1, 2, 3]}'::YAML, '$.items');
-- Returns: '[1, 2, 3]'

-- Objects are serialized
SELECT yaml_extract_string('{user: {name: John}}'::YAML, '$.user');
-- Returns: '{name: John}'
```

---

## yaml_exists

Checks if a path exists in a YAML document.

### Syntax

```sql
yaml_exists(yaml_value, path) → BOOLEAN
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_value` | YAML | The YAML value to query |
| `path` | VARCHAR | JSONPath-like path expression |

### Returns

BOOLEAN - true if path exists, false otherwise.

### Examples

```sql
-- Check property exists
SELECT yaml_exists('{name: John, age: 30}'::YAML, '$.name');
-- Returns: true

SELECT yaml_exists('{name: John}'::YAML, '$.age');
-- Returns: false

-- Check array index
SELECT yaml_exists('[1, 2, 3]'::YAML, '$[1]');
-- Returns: true

SELECT yaml_exists('[1, 2, 3]'::YAML, '$[5]');
-- Returns: false

-- Check nested path
SELECT yaml_exists('{user: {profile: {bio: Hello}}}'::YAML, '$.user.profile.bio');
-- Returns: true

-- Check null values (exists but null)
SELECT yaml_exists('{value: null}'::YAML, '$.value');
-- Returns: true
```

---

## yaml_type

Returns the type of a YAML value or value at a path.

### Syntax

```sql
yaml_type(yaml_value) → VARCHAR
yaml_type(yaml_value, path) → VARCHAR
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_value` | YAML | The YAML value to query |
| `path` | VARCHAR | Optional path expression |

### Returns

VARCHAR - One of: `'object'`, `'array'`, `'scalar'`, `'null'`

### Examples

```sql
-- Type of root value
SELECT yaml_type('{name: John}'::YAML);
-- Returns: 'object'

SELECT yaml_type('[1, 2, 3]'::YAML);
-- Returns: 'array'

SELECT yaml_type('Hello World'::YAML);
-- Returns: 'scalar'

SELECT yaml_type('null'::YAML);
-- Returns: 'null'

-- Type at path
SELECT yaml_type('{items: [1, 2, 3]}'::YAML, '$.items');
-- Returns: 'array'

SELECT yaml_type('{user: {name: John}}'::YAML, '$.user');
-- Returns: 'object'

SELECT yaml_type('{count: 42}'::YAML, '$.count');
-- Returns: 'scalar'
```

---

## yaml_keys

Returns the keys of a YAML object.

### Syntax

```sql
yaml_keys(yaml_value) → VARCHAR[]
yaml_keys(yaml_value, path) → VARCHAR[]
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_value` | YAML | The YAML value to query |
| `path` | VARCHAR | Optional path to an object |

### Returns

VARCHAR[] - Array of key names, or NULL if not an object.

### Examples

```sql
-- Keys of root object
SELECT yaml_keys('{name: John, age: 30, city: NYC}'::YAML);
-- Returns: [name, age, city]

-- Keys at path
SELECT yaml_keys('{user: {id: 1, email: a@b.com}}'::YAML, '$.user');
-- Returns: [id, email]

-- Empty object
SELECT yaml_keys('{}'::YAML);
-- Returns: []

-- Non-object returns NULL
SELECT yaml_keys('[1, 2, 3]'::YAML);
-- Returns: NULL
```

---

## yaml_array_length

Returns the length of a YAML array.

### Syntax

```sql
yaml_array_length(yaml_value) → BIGINT
yaml_array_length(yaml_value, path) → BIGINT
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_value` | YAML | The YAML value to query |
| `path` | VARCHAR | Optional path to an array |

### Returns

BIGINT - Number of elements, or NULL if not an array.

### Examples

```sql
-- Length of root array
SELECT yaml_array_length('[1, 2, 3, 4, 5]'::YAML);
-- Returns: 5

-- Length at path
SELECT yaml_array_length('{items: [a, b, c]}'::YAML, '$.items');
-- Returns: 3

-- Empty array
SELECT yaml_array_length('[]'::YAML);
-- Returns: 0

-- Non-array returns NULL
SELECT yaml_array_length('{name: John}'::YAML);
-- Returns: NULL
```

---

## yaml_each

Table function that returns key-value pairs from a YAML object.

### Syntax

```sql
yaml_each(yaml_value) → TABLE(key VARCHAR, value YAML)
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_value` | YAML | A YAML object |

### Returns

TABLE with columns:

- `key` VARCHAR - The key name
- `value` YAML - The value

### Examples

```sql
-- Iterate over object
SELECT * FROM yaml_each('{name: John, age: 30, active: true}'::YAML);
-- Returns:
-- key    | value
-- -------|-------
-- name   | John
-- age    | 30
-- active | true

-- Use in queries
SELECT key, yaml_type(value) AS type
FROM yaml_each('{str: hello, num: 42, arr: [1,2,3]}'::YAML);
-- Returns:
-- key | type
-- ----|------
-- str | scalar
-- num | scalar
-- arr | array
```

---

## yaml_array_elements

Table function that unnests YAML array elements into rows.

### Syntax

```sql
yaml_array_elements(yaml_value) → TABLE(value YAML)
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_value` | YAML | A YAML array |

### Returns

TABLE with column:

- `value` YAML - Each array element

### Examples

```sql
-- Unnest simple array
SELECT * FROM yaml_array_elements('[1, 2, 3]'::YAML);
-- Returns:
-- value
-- -----
-- 1
-- 2
-- 3

-- Unnest array of objects
SELECT value, yaml_extract_string(value, '$.name') AS name
FROM yaml_array_elements('[{name: Alice}, {name: Bob}]'::YAML);
-- Returns:
-- value         | name
-- --------------|------
-- {name: Alice} | Alice
-- {name: Bob}   | Bob

-- Combine with other operations
SELECT
    yaml_extract_string(value, '$.product') AS product,
    yaml_extract(value, '$.quantity')::INTEGER AS qty
FROM yaml_array_elements(yaml_extract(order_data, '$.items'));
```

---

## Path Expression Reference

| Pattern | Description | Example |
|---------|-------------|---------|
| `$` | Root element | `$` |
| `$.key` | Object key | `$.name` |
| `$['key']` | Object key (alternative) | `$['name']` |
| `$[n]` | Array index | `$[0]` |
| `$.a.b` | Chained access | `$.user.email` |
| `$[0].key` | Mixed access | `$[0].name` |

## See Also

- [Conversion Functions](conversion.md) - Converting YAML types
- [Parsing Functions](parsing.md) - Parsing YAML strings
- [YAML Type Guide](../guide/yaml-type.md) - Working with YAML type
