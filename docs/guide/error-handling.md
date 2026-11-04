# Error Handling

The YAML extension provides robust error handling for dealing with malformed data and edge cases.

## Error Recovery

### ignore_errors Parameter

Continue processing despite errors in some files or documents:

```sql
-- Stop on first error (default)
SELECT * FROM read_yaml('data/*.yaml', ignore_errors = false);

-- Skip invalid files/documents and continue
SELECT * FROM read_yaml('data/*.yaml', ignore_errors = true);
```

### Behavior by Error Type

| Error Type | ignore_errors=false | ignore_errors=true |
|------------|--------------------|--------------------|
| File not found | Error raised | File skipped |
| Permission denied | Error raised | File skipped |
| Invalid YAML syntax | Error raised | Document skipped |
| Type conversion failure | Error raised | Value becomes NULL |

### Multi-Document Error Handling

With multi-document YAML files, errors are isolated per document:

```yaml
---
valid: document
---
invalid: yaml: syntax
---
another: valid
```

```sql
-- With ignore_errors=true: returns 2 rows (skips invalid document)
SELECT * FROM read_yaml('mixed.yaml', ignore_errors = true);
```

## Validation Functions

### yaml_valid

Check if a string is valid YAML:

```sql
-- Valid YAML
SELECT yaml_valid('name: John');  -- true

-- Invalid YAML
SELECT yaml_valid('name: : bad: syntax');  -- false

-- NULL handling
SELECT yaml_valid(NULL::VARCHAR);  -- NULL
```

### Validating Before Processing

```sql
-- Filter out invalid YAML
SELECT *
FROM raw_data
WHERE yaml_valid(yaml_column);

-- Identify invalid entries
SELECT id, yaml_column
FROM raw_data
WHERE NOT yaml_valid(yaml_column);
```

## Common Error Scenarios

### File Access Errors

```sql
-- Missing file
SELECT * FROM read_yaml('nonexistent.yaml');
-- IO Error: File or directory does not exist: nonexistent.yaml

-- With error recovery
SELECT * FROM read_yaml(['exists.yaml', 'nonexistent.yaml'], ignore_errors = true);
-- Returns data from exists.yaml only
```

### Syntax Errors

Common YAML syntax issues:

```yaml
# Incorrect indentation
parent:
child: value  # Should be indented

# Unquoted special characters
description: This has a : colon  # Colon needs quoting

# Tab characters (YAML requires spaces)
key:	value  # Tab character causes error
```

### Type Mismatch

When explicit types don't match data:

```sql
SELECT * FROM read_yaml('data.yaml', columns = {
    'count': 'INTEGER'
});
-- Error if 'count' contains non-numeric values like "N/A"
```

Solution: Use VARCHAR for flexible fields, then cast:

```sql
SELECT
    CASE
        WHEN count_str ~ '^\d+$' THEN count_str::INTEGER
        ELSE NULL
    END AS count
FROM read_yaml('data.yaml', columns = {'count_str': 'VARCHAR'});
```

### Schema Inconsistency

Files with different structures:

```yaml
# file1.yaml
name: Alice
age: 30

# file2.yaml
name: Bob
department: Engineering
```

```sql
-- Merged schema with NULLs for missing fields
SELECT name, age, department
FROM read_yaml(['file1.yaml', 'file2.yaml']);
-- Returns:
-- Alice, 30, NULL
-- Bob, NULL, Engineering
```

## Error Messages

The extension provides descriptive error messages:

```
IO Error: File or directory does not exist: nonexistent.yaml
Invalid Input Error: Failed to parse YAML: yaml-cpp: error at line 1, column 10: illegal map value
Invalid Input Error: Cannot read "x" as BIGINT. A column type comes from a sample of the rows. ...
```

## Defensive Patterns

### Validate Input Files

```sql
-- Check files before processing
CREATE OR REPLACE MACRO check_yaml_files(pattern) AS TABLE
SELECT
    filename AS file,
    yaml_valid(content) AS is_valid
FROM read_text(pattern);

-- Use it
SELECT * FROM check_yaml_files('data/*.yaml')
WHERE NOT is_valid;
```

### Safe Type Conversion

```sql
-- Use TRY_CAST for safe conversion
SELECT
    yaml_extract_string(data, '$.name') AS name,
    TRY_CAST(yaml_extract_string(data, '$.age') AS INTEGER) AS age
FROM yaml_data;
-- Returns NULL for age if conversion fails
```

### Coalesce Missing Values

```sql
SELECT
    COALESCE(yaml_extract_string(config, '$.host'), 'localhost') AS host,
    COALESCE(yaml_extract(config, '$.port')::INTEGER, 8080) AS port
FROM configs;
```

### Check Path Existence

```sql
SELECT
    CASE
        WHEN yaml_exists(config, '$.database.host')
        THEN yaml_extract_string(config, '$.database.host')
        ELSE 'default-host'
    END AS db_host
FROM configs;
```

## Logging and Debugging

### Identify Problematic Files

```sql
-- Find files that can't be parsed
SELECT filename
FROM (
    SELECT filename, yaml_valid(content) AS valid
    FROM (SELECT filename, content::YAML AS data FROM read_text('data/*.yaml'))
)
WHERE NOT valid;
```

### Debug YAML Structure

```sql
-- Inspect YAML types at various paths
SELECT
    yaml_type(data) AS root_type,
    yaml_type(data, '$.items') AS items_type,
    yaml_keys(data) AS top_level_keys
FROM (SELECT filename, content::YAML AS data FROM read_text('config.yaml'));
```

## Best Practices

### For Production Pipelines

1. **Validate early**: Check YAML validity before complex processing
2. **Use explicit schemas**: Define `columns` parameter for consistency
3. **Handle missing data**: Use COALESCE and CASE for optional fields
4. **Log errors**: Track which files/documents failed for review

### For Data Exploration

1. **Enable ignore_errors**: Continue processing despite issues
2. **Check schema**: Use `DESCRIBE` to understand detected types
3. **Sample first**: Read a subset before processing large datasets

### For ETL Jobs

```sql
-- Robust ETL pattern
CREATE TABLE processed_data AS
SELECT
    COALESCE(yaml_extract_string(data, '$.id'), 'unknown') AS id,
    yaml_extract_string(data, '$.name') AS name,
    TRY_CAST(yaml_extract_string(data, '$.value') AS DOUBLE) AS value,
    filename AS source_file
FROM (SELECT filename, TRY_CAST(content AS YAML) AS data FROM read_text('input/*.yaml'))
WHERE yaml_valid(data)
  AND yaml_exists(data, '$.id');
```

## See Also

- [Reading YAML Files](reading-yaml.md) - File reading parameters
- [Extraction Functions](../functions/extraction.md) - Safe value extraction
- [Type Detection](type-detection.md) - Understanding auto-detection
