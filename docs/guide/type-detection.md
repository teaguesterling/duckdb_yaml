# Type Detection

The YAML extension infers DuckDB types from YAML values. It follows the [YAML 1.2 core schema](https://yaml.org/spec/1.2.2/#103-core-schema): a plain scalar that the schema does not call a null, a boolean or a number is a string. Dates and times are DuckDB's addition, as in `read_json`.

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

Detected format: ISO 8601, `2024-01-15`.

Other forms, such as `01/15/2024` and `15.01.2024`, read as VARCHAR. To parse them, use `strptime`.

```yaml
date1: 2024-01-15  # DATE
date2: 01/15/2024  # VARCHAR
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

A timestamp with `Z` or a UTC offset is `TIMESTAMP WITH TIME ZONE`, so it keeps its instant: `2024-01-15T14:30:00+05:00` is `2024-01-15 09:30:00+00`. Without one, it is `TIMESTAMP`.

`read_json` sets the rule for these values. A value that would lose information stays VARCHAR, so the text is kept:

```yaml
a: 2024-01-15 14:30:00 +05:00           # VARCHAR: a space before the offset
b: 2024-01-15 14:30:00 America/New_York # VARCHAR: a time zone name
c: 2024-01-15T14:30                     # VARCHAR: no seconds and no offset
d: 2024-01-15T24:00:00                  # VARCHAR: hour 24 needs an offset or Z
```

With `columns`, a `TIMESTAMP` or `TIMESTAMP WITH TIME ZONE` column applies an offset and accepts `UTC`. Any other time zone name is a type mismatch.

`epoch` and `infinity` stay VARCHAR, quoted or not. `read_json` reads them as dates.

#### Dates in other orders

A column also becomes `DATE` or `TIMESTAMP` when one format fits every sampled value. The formats are the ones `read_json` uses:

| Type | Formats, most preferred first |
|------|------|
| DATE | `%y-%m-%d`, `%d-%m-%y`, `%d-%m-%Y`, `%m-%d-%y`, `%m-%d-%Y` |
| TIMESTAMP | `%y-%m-%d %H:%M:%S`, `%d-%m-%y %H:%M:%S`, `%d-%m-%Y %H:%M:%S`, `%m-%d-%y %I:%M:%S %p`, `%m-%d-%Y %I:%M:%S %p` |

The column keeps one format for all of its values, so one row can settle the order for the others:

```yaml
- a: 11-03-2026  # 2026-11-03: month first, because of the next row
- a: 01-13-2026  # 2026-01-13
```

When several formats fit, the column takes the most preferred one, as `read_json` does. **Check columns of this kind.** A column that holds only `11-03-2026` reads as March 11. A code such as `1-2-3` reads as `2001-02-03`. To keep the text, set the column to `VARCHAR` with `columns`. To choose the order, read it as `VARCHAR` and use `strptime`.

If no single format fits, the column stays text. A later value that does not fit the format from the sample is an error, or NULL with `ignore_errors`. `read_json` differs here: it can read two rows of one column in different orders.

`yaml_to_json` has no column to look at, so it leaves these values as text.

#### UUID

A hyphenated UUID is `UUID`, as in `read_json`. Other forms, such as a UUID with no hyphens or in braces, are VARCHAR unless the column type is set.

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
```

Integers can be decimal with an optional sign, `0o17` (octal, 15) or `0x1F` (hexadecimal, 31). A leading zero does not mean octal: `007` is 7.

A fraction or an exponent makes a float, even when the value is whole: `1.0` and `1e3` are `DOUBLE`.

A value that DuckDB cannot hold without changing it, such as `9223372036854775808` or `1e400`, is `VARCHAR`, so the text is kept. Other forms, such as `1_000`, `0X1F` and `0b101`, are not core numbers and are also `VARCHAR`.

#### Special Float Values

The core forms are `.inf`, `.Inf`, `.INF`, `.nan`, `.NaN` and `.NAN`, and infinity can have a sign. Other spellings, such as `inf` and `nan`, are `VARCHAR`.

```yaml
infinity: .inf
negative_infinity: -.inf
not_a_number: .nan
word: inf          # VARCHAR
```

### Boolean Type

The core forms are `true`, `True`, `TRUE`, `false`, `False` and `FALSE`.

YAML 1.1 also used `yes`, `no`, `on`, `off`, `y` and `n`. In YAML 1.2 these are strings, so `country: NO` is the string `NO`. To read them as booleans, set the column type:

```sql
SELECT * FROM read_yaml('data.yaml', columns = {'enabled': 'BOOLEAN'});
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

### Quoted Values

Quotes decide the type. A quoted or block scalar is a string, so it never becomes a number, boolean or NULL:

```yaml
zip: "01234"       # VARCHAR '01234'
zip: 01234         # SMALLINT 1234
country: "NO"      # VARCHAR 'NO'
country: NO        # VARCHAR 'NO', also with no quotes
missing: "null"    # VARCHAR 'null'
missing: null      # NULL
```

Dates and times are the exception. As in `read_json`, a quoted date or time is still detected:

```yaml
day: "2024-01-15"  # DATE
at: "14:30"        # TIME
```

To keep these values as text, set `auto_detect = false` or set the column type with `columns`. See [Controlling Type Detection](#controlling-type-detection).

Writing keeps this rule. `value_to_yaml` and `COPY ... (FORMAT yaml)` quote a VARCHAR when its plain form could read back as a different type, in this extension or in another YAML parser. So `'2024-01-15'`, `'14:30'` and `'yes'` are written with quotes.

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
    A value after the sample that the column type cannot hold is an error, and the error names the value.
    Increase sampling parameters, use explicit `columns`, or set `ignore_errors = true` to read it as NULL.
    Integer columns are `BIGINT` when the sample stops before the end of the input, so a larger integer is not an error.

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
