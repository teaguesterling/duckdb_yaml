# Reading YAML Files

The YAML extension provides flexible options for reading YAML files into DuckDB tables.

## Direct File Access

The simplest way to query YAML files is using the file path directly in the `FROM` clause:

```sql
-- Single file
SELECT * FROM 'config.yaml';

-- Glob pattern
SELECT * FROM 'data/*.yaml';

-- Both .yaml and .yml extensions work
SELECT * FROM 'settings.yml';
```

## read_yaml Function

For more control, use the `read_yaml` table function:

```sql
SELECT * FROM read_yaml('path/to/file.yaml');
```

### Input Types

The function accepts several input formats:

=== "Single File"

    ```sql
    SELECT * FROM read_yaml('config.yaml');
    ```

=== "Glob Pattern"

    ```sql
    SELECT * FROM read_yaml('data/*.yaml');
    SELECT * FROM read_yaml('configs/**/*.yml');
    ```

=== "File List"

    ```sql
    SELECT * FROM read_yaml(['config1.yaml', 'config2.yaml', 'config3.yaml']);
    ```

=== "Directory"

    ```sql
    -- Reads all .yaml and .yml files in the directory
    SELECT * FROM read_yaml('data/configs/');
    ```

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `auto_detect` | BOOLEAN | `true` | Enable automatic type detection |
| `columns` | STRUCT | - | Explicit column type specification |
| `records` | VARCHAR | - | Path to nested array of records (dot notation) |
| `multi_document` | BOOLEAN | `true` | Handle multiple YAML documents |
| `expand_root_sequence` | BOOLEAN | `true` | Expand top-level sequences into rows |
| `ignore_errors` | BOOLEAN | `false` | Continue on parsing errors |
| `maximum_object_size` | INTEGER | 16777216 | Maximum file size in bytes (16MB) |
| `sample_size` | INTEGER | 20480 | Rows to sample for schema detection |
| `maximum_sample_files` | INTEGER | 32 | Files to sample for schema detection |

### auto_detect

Controls automatic type detection for columns:

```sql
-- Enabled (default): dates, times, numbers detected automatically
SELECT * FROM read_yaml('data.yaml', auto_detect = true);

-- Disabled: all values remain as VARCHAR
SELECT * FROM read_yaml('data.yaml', auto_detect = false);
```

### columns

Specify explicit column types:

```sql
SELECT * FROM read_yaml('data.yaml', columns = {
    'id': 'INTEGER',
    'name': 'VARCHAR',
    'created_at': 'TIMESTAMP',
    'metadata': 'JSON'
});
```

See [Column Type Specification](#column-type-specification) for details.

### records

Extract records from a nested array within the YAML structure using dot notation:

```yaml
# config.yaml
metadata:
  version: "1.0"
  author: admin
data:
  users:
    - id: 1
      name: Alice
      role: admin
    - id: 2
      name: Bob
      role: user
```

```sql
-- Extract the nested users array as rows
SELECT * FROM read_yaml('config.yaml', records = 'data.users');
```

| id | name | role |
|----|------|------|
| 1 | Alice | admin |
| 2 | Bob | user |

The `records` parameter supports:

- **Single-level paths**: `records = 'items'`
- **Nested paths**: `records = 'data.users'` or `records = 'config.database.tables'`
- **Unicode keys**: `records = 'données.éléments'`

!!! tip "When to use records"
    Use `records` when your YAML file contains metadata at the top level
    and the actual data records are nested within a specific field.

### multi_document

Handle files with multiple YAML documents:

```yaml
---
id: 1
name: First
---
id: 2
name: Second
```

```sql
-- Each document becomes a row (default)
SELECT * FROM read_yaml('multi.yaml', multi_document = true);

-- Disable to read only the first document
SELECT * FROM read_yaml('multi.yaml', multi_document = false);
```

### expand_root_sequence

Control how top-level sequences are handled:

```yaml
- name: Alice
  age: 30
- name: Bob
  age: 25
```

```sql
-- Each item becomes a row (default)
SELECT * FROM read_yaml('people.yaml', expand_root_sequence = true);
-- Returns: 2 rows

-- Keep as a single row with array
SELECT * FROM read_yaml('people.yaml', expand_root_sequence = false);
-- Returns: 1 row with array column
```

### ignore_errors

Continue processing when some files or documents have errors:

```sql
-- Stop on first error (default)
SELECT * FROM read_yaml('data/*.yaml', ignore_errors = false);

-- Skip invalid files/documents
SELECT * FROM read_yaml('data/*.yaml', ignore_errors = true);
```

### Sampling Parameters

Control schema detection for large datasets:

```sql
-- Sample more rows for complex schemas
SELECT * FROM read_yaml('data/*.yaml', sample_size = 50000);

-- Sample more files
SELECT * FROM read_yaml('data/*.yaml', maximum_sample_files = 100);

-- Unlimited sampling (reads all data for schema detection)
SELECT * FROM read_yaml('data/*.yaml',
    sample_size = -1,
    maximum_sample_files = -1
);
```

!!! tip "When to increase sampling"
    If you see type mismatch errors, some files may have different schemas.
    Increase `sample_size` or `maximum_sample_files` to capture all variations.

## Column Type Specification

Explicitly define column types to ensure consistent schemas:

### Basic Types

```sql
SELECT * FROM read_yaml('data.yaml', columns = {
    'id': 'INTEGER',
    'name': 'VARCHAR',
    'price': 'DECIMAL(10,2)',
    'active': 'BOOLEAN',
    'created': 'TIMESTAMP'
});
```

### Complex Types

```sql
SELECT * FROM read_yaml('config.yaml', columns = {
    'server': 'STRUCT(host VARCHAR, port INTEGER)',
    'features': 'VARCHAR[]',
    'settings': 'MAP(VARCHAR, VARCHAR)'
});
```

### Preserving YAML Structure

```sql
SELECT * FROM read_yaml('nested.yaml', columns = {
    'config': 'YAML',    -- Keep as YAML for later processing
    'metadata': 'JSON'   -- Convert to JSON type
});
```

### Partial Specification

Specify some columns, let others be auto-detected:

```sql
SELECT * FROM read_yaml('data.yaml',
    auto_detect = true,
    columns = {
        'id': 'BIGINT',      -- Force this to BIGINT
        'name': 'VARCHAR'     -- Force this to VARCHAR
    }
    -- Other columns auto-detected
);
```

## read_yaml_objects

For preserving the complete document structure (one row per file):

```sql
SELECT * FROM read_yaml_objects('config.yaml');
```

This is useful when you want to:

- Keep the entire YAML as a single value
- Process complex nested structures
- Maintain document boundaries

## Multi-Document YAML

Files with multiple YAML documents (separated by `---`) are handled automatically:

```yaml
---
id: 1
type: user
name: Alice
---
id: 2
type: admin
name: Bob
```

```sql
SELECT * FROM read_yaml('users.yaml');
-- Returns 2 rows
```

## Sequence Handling

Top-level sequences are expanded into rows:

```yaml
- name: Alice
  score: 95
- name: Bob
  score: 87
- name: Carol
  score: 92
```

```sql
SELECT * FROM read_yaml('scores.yaml');
-- Returns 3 rows with columns: name, score
```

## Examples

### Reading Configuration Files

```yaml
# app.yaml
database:
  host: localhost
  port: 5432
  name: myapp
cache:
  enabled: true
  ttl: 3600
```

```sql
SELECT
    database,
    yaml_extract_string(database, '$.host') AS db_host,
    yaml_extract(database, '$.port') AS db_port,
    cache
FROM read_yaml('app.yaml');
```

### Combining Multiple Files

```sql
-- Read all configs and include filename
SELECT
    filename,
    yaml_extract_string(data, '$.name') AS name,
    yaml_extract(data, '$.version') AS version
FROM read_yaml_objects('configs/*.yaml');
```

### Creating Tables from YAML

```sql
-- Create a table from YAML with explicit types
CREATE TABLE events AS
SELECT * FROM read_yaml('events.yaml', columns = {
    'id': 'INTEGER',
    'timestamp': 'TIMESTAMP',
    'type': 'VARCHAR',
    'data': 'JSON'
});
```

## See Also

- [Frontmatter Extraction](frontmatter.md) - Reading YAML from Markdown files
- [Type Detection](type-detection.md) - How types are automatically detected
- [Error Handling](error-handling.md) - Handling malformed YAML
