# YAML Extraction Functions

The YAML extension provides functions for extracting and querying data from YAML values, similar to the JSON extraction functions.

## Functions

### yaml_type

Returns the type of a YAML value.

**Syntax:**
```sql
-- Get the type of a YAML value
yaml_type(yaml_value)

-- Get the type at a specific path
yaml_type(yaml_value, path)
```

**Returns:** VARCHAR - one of: 'object', 'array', 'scalar', 'null'

**Examples:**
```sql
SELECT yaml_type('{"name": "John"}');  -- Returns: 'object'
SELECT yaml_type('[1, 2, 3]');         -- Returns: 'array'
SELECT yaml_type('42');                -- Returns: 'scalar'
SELECT yaml_type('null');              -- Returns: 'null'

-- With path
SELECT yaml_type('{"user": {"name": "John"}}', '$.user');  -- Returns: 'object'
SELECT yaml_type('[1, [2, 3]]', '$[1]');                   -- Returns: 'array'
```

### yaml_extract

Extracts a value from a YAML document at the specified path.

**Syntax:**
```sql
yaml_extract(yaml_value, path)
```

**Returns:** YAML - the extracted value as YAML

**Examples:**
```sql
SELECT yaml_extract('{"name": "John", "age": 30}', '$.name');      -- Returns: 'John'
SELECT yaml_extract('{"user": {"id": 123}}', '$.user.id');         -- Returns: '123'
SELECT yaml_extract('[1, 2, [3, 4]]', '$[2]');                     -- Returns: '[3, 4]'
SELECT yaml_extract('[1, 2, [3, 4]]', '$[2][0]');                  -- Returns: '3'
```

### yaml_extract_string

Extracts a value from a YAML document as a string.

**Syntax:**
```sql
yaml_extract_string(yaml_value, path)
```

**Returns:** VARCHAR - the extracted value as a string, or NULL if path not found

**Examples:**
```sql
SELECT yaml_extract_string('{"name": "John"}', '$.name');          -- Returns: 'John'
SELECT yaml_extract_string('{"count": 42}', '$.count');            -- Returns: '42'
SELECT yaml_extract_string('{"obj": {"a": 1}}', '$.obj');          -- Returns: '{"a": 1}'
SELECT yaml_extract_string('{"name": "John"}', '$.missing');       -- Returns: NULL
```

### yaml_exists

Checks if a path exists in a YAML document.

**Syntax:**
```sql
yaml_exists(yaml_value, path)
```

**Returns:** BOOLEAN - true if the path exists, false otherwise

**Examples:**
```sql
SELECT yaml_exists('{"name": "John"}', '$.name');           -- Returns: true
SELECT yaml_exists('{"name": "John"}', '$.age');            -- Returns: false
SELECT yaml_exists('[1, 2, 3]', '$[1]');                    -- Returns: true
SELECT yaml_exists('[1, 2, 3]', '$[5]');                    -- Returns: false
```

## Path Syntax

The path syntax follows similar conventions to JSONPath:
- `$` - Root of the document
- `.key` - Access object property
- `[n]` - Access array element by index
- `.` - Chain property access

Examples:
- `$.name` - Access 'name' property at root
- `$.user.email` - Access nested property
- `$[0]` - First element of root array
- `$.items[2].price` - Price of third item

## Use Cases

### Filtering Based on YAML Content
```sql
SELECT * FROM yaml_table 
WHERE yaml_exists(data, '$.active') 
  AND yaml_extract_string(data, '$.active') = 'true';
```

### Extracting Nested Values
```sql
SELECT 
    yaml_extract_string(data, '$.user.name') as user_name,
    yaml_extract_string(data, '$.user.email') as email,
    yaml_extract(data, '$.user.settings') as settings
FROM yaml_documents;
```

### Type-based Processing
```sql
SELECT 
    CASE yaml_type(data, '$.value')
        WHEN 'array' THEN 'Process as list'
        WHEN 'object' THEN 'Process as map'
        ELSE 'Process as scalar'
    END as processing_type
FROM yaml_data;
```