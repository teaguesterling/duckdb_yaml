# Quick Start Tutorial

This tutorial walks through the core features of the YAML extension with practical examples.

## Setup

First, install and load the extension:

```sql
INSTALL yaml FROM community;
LOAD yaml;
```

## Reading YAML Files

### Direct File Access

The simplest way to query YAML is using direct file paths:

```sql
-- Query a single file
SELECT * FROM 'config.yaml';

-- Query multiple files with glob patterns
SELECT * FROM 'data/*.yaml';

-- Filter results
SELECT name, version FROM 'package.yaml' WHERE version > '1.0';
```

### Using read_yaml

For more control, use the `read_yaml` function:

```sql
-- Basic usage
SELECT * FROM read_yaml('config.yaml');

-- With options
SELECT * FROM read_yaml('data/*.yaml',
    auto_detect = true,
    ignore_errors = true
);

-- With explicit column types
SELECT * FROM read_yaml('data.yaml', columns = {
    'id': 'INTEGER',
    'name': 'VARCHAR',
    'created': 'TIMESTAMP'
});
```

### Example: Reading a Configuration File

Given `config.yaml`:

```yaml
database:
  host: localhost
  port: 5432
  name: myapp
features:
  - authentication
  - logging
settings:
  timeout: 30
  retries: 3
```

Query it with:

```sql
SELECT * FROM 'config.yaml';
```

## Working with YAML Data

### Creating YAML Columns

```sql
-- Create a table with YAML column
CREATE TABLE app_configs (
    app_name VARCHAR,
    config YAML
);

-- Insert YAML data
INSERT INTO app_configs VALUES
    ('web', 'port: 8080\nworkers: 4'),
    ('api', 'port: 3000\nrateLimit: 100');

-- Query the data
SELECT
    app_name,
    yaml_extract(config, '$.port') AS port
FROM app_configs;
```

### Extracting Values

```sql
-- Extract specific values
SELECT yaml_extract_string(config, '$.database.host') AS db_host
FROM 'config.yaml';

-- Check if paths exist
SELECT yaml_exists(config, '$.features') AS has_features
FROM 'config.yaml';

-- Get value types
SELECT yaml_type(config, '$.settings') AS settings_type
FROM 'config.yaml';
```

## Extracting Frontmatter

Query YAML frontmatter from Markdown files:

```sql
-- Read blog post metadata
SELECT title, author, date, tags
FROM read_yaml_frontmatter('posts/*.md');

-- Include the content body
SELECT title, content
FROM read_yaml_frontmatter('posts/*.md', content := true);

-- Filter by metadata
SELECT title, date
FROM read_yaml_frontmatter('posts/*.md')
WHERE list_contains(tags, 'tutorial')
ORDER BY date DESC;
```

## Converting Between Types

```sql
-- YAML to JSON
SELECT yaml_to_json('name: John\nage: 30'::YAML);
-- Returns: {"name":"John","age":30}

-- JSON to YAML
SELECT '{"name":"John"}'::JSON::YAML;
-- Returns: {name: John}

-- DuckDB values to YAML
SELECT value_to_yaml({'name': 'John', 'scores': [85, 90, 95]});
-- Returns: name: John\nscores:\n  - 85\n  - 90\n  - 95
```

## Writing YAML Files

Export query results to YAML:

```sql
-- Basic export
COPY (SELECT * FROM users) TO 'users.yaml' (FORMAT yaml);

-- With block style (more readable)
COPY (SELECT * FROM users) TO 'users.yaml' (FORMAT yaml, STYLE block);

-- As a sequence
COPY (SELECT * FROM users) TO 'users.yaml' (FORMAT yaml, LAYOUT sequence);
```

## Advanced Queries

### Unnesting YAML Arrays

```sql
-- Expand array elements into rows
SELECT * FROM yaml_array_elements('[1, 2, 3]'::YAML);
-- Returns: 1, 2, 3 (as separate rows)

-- Iterate over object key-value pairs
SELECT * FROM yaml_each('{name: John, age: 30}'::YAML);
-- Returns: (name, John), (age, 30)
```

### Building YAML Objects

```sql
-- Construct YAML from values
SELECT yaml_build_object('name', 'John', 'age', 30);
-- Returns: {name: John, age: 30}
```

### Aggregating Data

```sql
-- Get array length
SELECT yaml_array_length('[1, 2, 3, 4, 5]'::YAML);
-- Returns: 5

-- Get object keys
SELECT yaml_keys('{a: 1, b: 2, c: 3}'::YAML);
-- Returns: [a, b, c]
```

## Next Steps

Now that you've seen the basics, explore:

- [Reading YAML Files](../guide/reading-yaml.md) - Advanced file reading options
- [YAML Type](../guide/yaml-type.md) - Working with the YAML data type
- [Functions Reference](../functions/index.md) - Complete function documentation
- [Examples](../examples/index.md) - Real-world use cases
