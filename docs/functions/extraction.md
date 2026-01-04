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

## ->> Operator

The `->>` operator is an alias for `yaml_extract_string`, providing a more concise syntax for extracting values as strings.

### Syntax

```sql
yaml_value ->> path → VARCHAR
```

### Examples

```sql
-- Extract using arrow operator
SELECT '{name: John, age: 30}'::YAML ->> '$.name';
-- Returns: 'John'

-- Works with VARCHAR input (no cast needed)
SELECT 'name: Alice' ->> '$.name';
-- Returns: 'Alice'

-- Chain with yaml_extract for complex paths
SELECT yaml_extract(data, '$.user') ->> '$.email'
FROM my_table;
```

!!! note "The `->` Operator"
    The `->` operator (which would return YAML type) is not available because DuckDB's parser hardcodes it to `json_extract`. Use `yaml_extract()` function instead.

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

## yaml_structure

Returns a JSON representation of the YAML document's structure/schema, showing the detected types for each field.

### Syntax

```sql
yaml_structure(yaml_value) → JSON
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_value` | YAML or VARCHAR | The YAML value to analyze |

### Returns

JSON - A JSON object representing the structure with type names as values.

### Examples

```sql
-- Simple object structure
SELECT yaml_structure('name: Alice
age: 30');
-- Returns: {"name":"VARCHAR","age":"UBIGINT"}

-- Nested object
SELECT yaml_structure('user:
  name: Bob
  active: true');
-- Returns: {"user":{"name":"VARCHAR","active":"BOOLEAN"}}

-- Array structure
SELECT yaml_structure('[1, 2, 3]');
-- Returns: ["UBIGINT"]

-- Complex nested structure
SELECT yaml_structure('users:
  - name: Alice
    scores: [90, 85]
  - name: Bob
    scores: [88, 92]');
-- Returns: {"users":[{"name":"VARCHAR","scores":["UBIGINT"]}]}

-- Arrays of objects with different keys are merged
SELECT yaml_structure('[{a: 1}, {b: 2}]');
-- Returns: [{"a":"UBIGINT","b":"UBIGINT"}]
```

---

## yaml_contains

Checks if one YAML document contains another. Useful for filtering or matching YAML data.

### Syntax

```sql
yaml_contains(haystack, needle) → BOOLEAN
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `haystack` | YAML or VARCHAR | The YAML document to search in |
| `needle` | YAML or VARCHAR | The YAML pattern to search for |

### Returns

BOOLEAN - true if haystack contains needle, false otherwise.

### Containment Rules

- **Objects**: All key-value pairs in needle must exist in haystack
- **Arrays**: All elements in needle must be contained in haystack
- **Scalars**: Values must be equal
- **Nested**: Containment is checked recursively

### Examples

```sql
-- Object contains partial object
SELECT yaml_contains('a: 1
b: 2', 'a: 1');
-- Returns: true

-- Wrong value doesn't match
SELECT yaml_contains('a: 1', 'a: 2');
-- Returns: false

-- Missing key doesn't match
SELECT yaml_contains('a: 1', 'b: 1');
-- Returns: false

-- Array contains element
SELECT yaml_contains('[1, 2, 3]', '2');
-- Returns: true

-- Array contains subset
SELECT yaml_contains('[1, 2, 3, 4]', '[2, 3]');
-- Returns: true

-- Nested containment
SELECT yaml_contains('user:
  name: Alice
  age: 30', 'user:
  name: Alice');
-- Returns: true

-- Use for filtering
SELECT * FROM my_table
WHERE yaml_contains(config, 'enabled: true');
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
