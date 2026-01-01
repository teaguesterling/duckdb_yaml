# Type Detection

The YAML extension includes comprehensive automatic type detection, inferring appropriate DuckDB types from YAML values.

## Overview

When `auto_detect = true` (default), the extension analyzes YAML values and assigns optimal types:

```sql
-- With auto_detect (default)
SELECT typeof(date_col), typeof(count_col), typeof(active_col)
FROM read_yaml('data.yaml');
-- Returns: DATE, INTEGER, BOOLEAN
```

## Detected Types

### Temporal Types

#### DATE

Detected formats:

| Format | Example |
|--------|---------|
| ISO 8601 | `2024-01-15` |
| Slash (US) | `01/15/2024` |
| Slash (EU) | `15/01/2024` |
| Dot | `15.01.2024` |

```yaml
# All detected as DATE
date1: 2024-01-15
date2: 01/15/2024
date3: 15.01.2024
```

#### TIME

Detected formats:

| Format | Example |
|--------|---------|
| Standard | `14:30:00` |
| With milliseconds | `14:30:00.123` |
| Short | `14:30` |

```yaml
# All detected as TIME
time1: 14:30:00
time2: 14:30:00.123
time3: 09:00
```

#### TIMESTAMP

Detected formats:

| Format | Example |
|--------|---------|
| ISO 8601 with Z | `2024-01-15T14:30:00Z` |
| ISO 8601 with offset | `2024-01-15T14:30:00+05:00` |
| ISO 8601 no timezone | `2024-01-15T14:30:00` |
| Space separator | `2024-01-15 14:30:00` |

```yaml
# All detected as TIMESTAMP
ts1: 2024-01-15T14:30:00Z
ts2: 2024-01-15T14:30:00+05:00
ts3: 2024-01-15 14:30:00
```

### Numeric Types

Numbers are detected with optimal precision:

| Type | Range | Example |
|------|-------|---------|
| TINYINT | -128 to 127 | `42` |
| SMALLINT | -32,768 to 32,767 | `1000` |
| INTEGER | -2.1B to 2.1B | `100000` |
| BIGINT | Larger integers | `9223372036854775807` |
| DOUBLE | Decimals | `3.14159` |

```yaml
# Detected as TINYINT
small: 42

# Detected as INTEGER
medium: 100000

# Detected as BIGINT
large: 9223372036854775807

# Detected as DOUBLE
decimal: 3.14159
scientific: 1.23e-4
```

#### Special Float Values

```yaml
infinity: .inf
negative_infinity: -.inf
not_a_number: .nan
```

### Boolean Type

All recognized as BOOLEAN:

| True Values | False Values |
|-------------|--------------|
| `true` | `false` |
| `True` | `False` |
| `TRUE` | `FALSE` |
| `yes` | `no` |
| `Yes` | `No` |
| `YES` | `NO` |
| `on` | `off` |
| `On` | `Off` |
| `ON` | `OFF` |
| `y` | `n` |
| `Y` | `N` |
| `t` | `f` |
| `T` | `F` |

```yaml
# All detected as BOOLEAN
active: true
enabled: yes
running: on
confirmed: Y
```

### NULL Values

All recognized as NULL:

```yaml
empty1: null
empty2: Null
empty3: NULL
empty4: ~
empty5:      # Empty value
```

### String Type

Any value not matching the above patterns defaults to VARCHAR:

```yaml
name: John Doe
email: john@example.com
description: "A multi-line
  string value"
```

## Array Detection

Arrays are typed based on their elements:

### Homogeneous Arrays

```yaml
# INTEGER[]
numbers: [1, 2, 3, 4, 5]

# VARCHAR[]
names: [Alice, Bob, Carol]

# BOOLEAN[]
flags: [true, false, true]
```

### Mixed-Type Arrays

When elements have different types, the array falls back to VARCHAR[]:

```yaml
# VARCHAR[] (mixed types)
mixed: [1, "two", true]
```

### Empty Arrays

Empty arrays default to VARCHAR[]:

```yaml
# VARCHAR[]
empty: []
```

## Struct Detection

Nested objects are detected as STRUCT types:

```yaml
user:
  name: John
  age: 30
  email: john@example.com
# Detected as: STRUCT(name VARCHAR, age TINYINT, email VARCHAR)
```

## Controlling Type Detection

### Disable Auto-Detection

```sql
-- All columns become VARCHAR
SELECT * FROM read_yaml('data.yaml', auto_detect = false);
```

### Override Specific Columns

```sql
-- Override some types, auto-detect others
SELECT * FROM read_yaml('data.yaml',
    auto_detect = true,
    columns = {
        'id': 'BIGINT',           -- Force BIGINT instead of detected INTEGER
        'price': 'DECIMAL(10,2)'  -- Force DECIMAL instead of DOUBLE
    }
);
```

### Force All Column Types

```sql
-- Specify all column types explicitly
SELECT * FROM read_yaml('data.yaml',
    auto_detect = false,
    columns = {
        'id': 'INTEGER',
        'name': 'VARCHAR',
        'created': 'TIMESTAMP',
        'active': 'BOOLEAN'
    }
);
```

## Schema Sampling

For large datasets with multiple files, the extension samples a subset to determine schema:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `sample_size` | 20480 | Number of rows to sample |
| `maximum_sample_files` | 32 | Number of files to sample |

```sql
-- Increase sampling for complex schemas
SELECT * FROM read_yaml('data/*.yaml',
    sample_size = 50000,
    maximum_sample_files = 100
);

-- Sample all data (slow for large datasets)
SELECT * FROM read_yaml('data/*.yaml',
    sample_size = -1,
    maximum_sample_files = -1
);
```

!!! warning "Schema Mismatch Errors"
    If sampling misses files with different structures, you may see cast errors.
    Increase sampling parameters or use explicit `columns` to handle this.

## Type Coercion

When a column has mixed types across rows, DuckDB applies type coercion:

```yaml
# File 1
value: 42

# File 2
value: "not a number"
```

Result: `value` column becomes VARCHAR to accommodate both.

## Best Practices

### When to Use Auto-Detection

- Data has consistent, predictable types
- Quick exploration of unknown datasets
- Prototyping queries

### When to Specify Types

- Production pipelines requiring consistent schemas
- Data with ambiguous formats (dates that look like strings)
- Performance-critical applications
- Ensuring specific numeric precision

### Handling Date Ambiguity

Dates like `01/02/2024` are ambiguous (January 2 or February 1?):

```sql
-- Force a specific interpretation
SELECT * FROM read_yaml('dates.yaml', columns = {
    'date': 'DATE'
});

-- Or use strptime for explicit parsing
SELECT strptime(date_string, '%m/%d/%Y')::DATE
FROM read_yaml('dates.yaml', auto_detect = false);
```

## See Also

- [Reading YAML Files](reading-yaml.md) - The `columns` parameter
- [YAML Type](yaml-type.md) - Storing parsed YAML data
- [Parameters Reference](../reference/parameters.md) - All available parameters
