# New YAML Extension Features

## YAML Extraction Functions

Similar to JSON extraction functions, you can now query and extract data from YAML values:

### Available Functions

- `yaml_type(yaml_value [, path])` - Get the type of a YAML value
- `yaml_extract(yaml_value, path)` - Extract a value at a path
- `yaml_extract_string(yaml_value, path)` - Extract a value as a string
- `yaml_exists(yaml_value, path)` - Check if a path exists

### Example Usage

```sql
-- Create a table with YAML data
CREATE TABLE yaml_data AS SELECT 
    value_to_yaml({
        'name': 'John',
        'address': {
            'city': 'NYC',
            'zip': 10001
        },
        'scores': [90, 85, 92]
    }) as data;

-- Query the data
SELECT 
    yaml_type(data) as root_type,
    yaml_extract_string(data, '$.name') as name,
    yaml_extract_string(data, '$.address.city') as city,
    yaml_extract(data, '$.scores[1]') as second_score,
    yaml_exists(data, '$.phone') as has_phone
FROM yaml_data;
```

Result:
```
root_type | name | city | second_score | has_phone
----------|------|------|--------------|----------
object    | John | NYC  | 85           | false
```

### Path Syntax

The path syntax follows JSONPath conventions:
- `$` - Root of the document
- `.property` - Access object property
- `[n]` - Access array element by index
- Paths can be chained: `$.user.address.city`, `$.items[0].price`

## Column Type Specification

You can now specify explicit column types when reading YAML files:

### Syntax

```sql
SELECT * FROM read_yaml('file.yaml', 
    columns={
        'id': 'INTEGER',
        'name': 'VARCHAR',
        'created': 'TIMESTAMP',
        'data': 'STRUCT(x INTEGER, y INTEGER)'
    }
);
```

### Examples

#### Basic Types
```yaml
# person.yaml
name: John Doe
age: 30
salary: 75000.50
active: true
joined: 2023-01-15
```

```sql
SELECT * FROM read_yaml('person.yaml', 
    columns={
        'name': 'VARCHAR',
        'age': 'INTEGER',
        'salary': 'DOUBLE',
        'active': 'BOOLEAN',
        'joined': 'DATE'
    }
);
```

#### Complex Types
```yaml
# config.yaml
server:
  host: localhost
  ports: [8080, 8081]
  options:
    ssl: true
    timeout: 30
```

```sql
SELECT * FROM read_yaml('config.yaml',
    columns={
        'server': 'STRUCT(host VARCHAR, ports INTEGER[], options STRUCT(ssl BOOLEAN, timeout INTEGER))'
    }
);
```

### Type Casting

The extension will attempt to cast YAML values to the specified types. If casting fails, it may return NULL or an error depending on the types involved.

### Benefits

1. **Schema Consistency**: Ensure all YAML files are read with the same schema
2. **Type Safety**: Avoid type inference issues with mixed data
3. **Performance**: Skip auto-detection overhead when types are known
4. **Integration**: Better integration with typed queries and views

### Interaction with auto_detect

When `columns` is specified, it overrides `auto_detect` for those columns:

```sql
-- These columns will use explicit types, others will be auto-detected
SELECT * FROM read_yaml('data.yaml',
    auto_detect=true,
    columns={
        'id': 'INTEGER',
        'name': 'VARCHAR'
    }
);
```

## Full Feature Documentation

For complete documentation, see:
- [YAML Extraction Functions](yaml_extraction_functions.md)
- [Column Type Specification](yaml_column_types.md)