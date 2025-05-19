# YAML Column Type Specification

The YAML extension supports explicit column type specification when reading YAML files, providing more control over data types compared to auto-detection.

## Syntax

```sql
SELECT * FROM read_yaml('file.yaml', 
    columns={
        'column_name': 'TYPE',
        'another_column': 'ANOTHER_TYPE'
    }
);
```

## Parameters

- `columns`: A struct/map specifying column names and their DuckDB types

## Supported Types

All DuckDB types are supported, including:
- Basic types: `VARCHAR`, `INTEGER`, `BIGINT`, `DOUBLE`, `BOOLEAN`, `DATE`, `TIMESTAMP`
- Complex types: `STRUCT(...)`, `LIST(...)`, `MAP(...)`

## Examples

### Basic Type Specification

```yaml
# person.yaml
name: John Doe
age: 30
salary: 75000.50
active: true
joined: 2023-01-15
```

```sql
-- Read with explicit types
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

### Complex Type Specification

```yaml
# user.yaml
user:
  name: Alice
  roles: [admin, user, viewer]
  settings:
    theme: dark
    notifications: true
scores: [85, 90, 78]
```

```sql
-- Read with nested types
SELECT * FROM read_yaml('user.yaml',
    columns={
        'user': 'STRUCT(name VARCHAR, roles VARCHAR[], settings STRUCT(theme VARCHAR, notifications BOOLEAN))',
        'scores': 'INTEGER[]'
    }
);
```

### Type Casting

When explicit types are specified, DuckDB will attempt to cast the YAML values to the requested types:

```sql
-- Force all values to VARCHAR
SELECT * FROM read_yaml('data.yaml',
    columns={
        'id': 'VARCHAR',
        'count': 'VARCHAR',
        'active': 'VARCHAR'
    }
);
```

### Partial Column Selection

You can specify types for only a subset of columns:

```sql
-- Only specify types for some columns
SELECT * FROM read_yaml('data.yaml',
    columns={
        'id': 'INTEGER',
        'name': 'VARCHAR'
    }
);
-- Other columns will use auto-detection or default to VARCHAR
```

## Interaction with Other Parameters

### auto_detect
When `columns` is specified with explicit types, it overrides `auto_detect` for those columns:

```sql
-- auto_detect is false, but columns are explicitly typed
SELECT * FROM read_yaml('data.yaml',
    auto_detect=false,
    columns={
        'id': 'INTEGER',
        'name': 'VARCHAR'
    }
);
```

### Missing Columns
If a specified column doesn't exist in the YAML data, it will be filled with NULL values:

```sql
-- If 'city' doesn't exist in the YAML
SELECT * FROM read_yaml('data.yaml',
    columns={
        'name': 'VARCHAR',
        'age': 'INTEGER',
        'city': 'VARCHAR'  -- Will be NULL if not in data
    }
);
```

## Error Handling

### Invalid Type Names
```sql
-- This will error
SELECT * FROM read_yaml('data.yaml',
    columns={
        'name': 'INVALID_TYPE'  -- Error: Invalid type
    }
);
```

### Type Conversion Errors
If a value cannot be converted to the specified type, DuckDB will either:
- Attempt a best-effort conversion
- Return NULL for the value
- Throw an error (depending on the specific types involved)

## Use Cases

### Enforcing Schema Consistency
```sql
-- Ensure all files have the same schema
CREATE VIEW orders AS
SELECT * FROM read_yaml('orders_*.yaml',
    columns={
        'order_id': 'INTEGER',
        'customer_id': 'INTEGER',
        'amount': 'DECIMAL(10,2)',
        'order_date': 'DATE',
        'items': 'STRUCT(product_id INTEGER, quantity INTEGER)[]'
    }
);
```

### Working with Mixed-Type YAML
```sql
-- Handle YAML with inconsistent types
SELECT * FROM read_yaml('mixed_data.yaml',
    columns={
        'id': 'VARCHAR',     -- Some files have string IDs
        'value': 'DOUBLE',   -- Ensure numeric processing
        'timestamp': 'TIMESTAMP'
    }
);
```

### Integration with Other Features
```sql
-- Combine with other parameters
SELECT * FROM read_yaml('data/*.yaml',
    ignore_errors=true,
    maximum_object_size=10485760,  -- 10MB
    columns={
        'id': 'BIGINT',
        'data': 'JSON',  -- Store complex YAML as JSON
        'created': 'TIMESTAMP'
    }
);
```