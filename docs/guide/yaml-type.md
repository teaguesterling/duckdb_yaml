# YAML Type

The YAML extension provides a native `YAML` type for storing and manipulating YAML data within DuckDB.

## Overview

The YAML type is implemented as an alias for VARCHAR, enabling:

- Type-safe YAML operations
- Seamless conversion between YAML, JSON, and VARCHAR
- Integration with DuckDB's type system

## Creating YAML Values

### From Literals

```sql
-- Using cast
SELECT 'name: John\nage: 30'::YAML;

-- Using the YAML type in columns
CREATE TABLE configs (
    id INTEGER,
    config YAML
);
```

### From DuckDB Values

```sql
-- Convert structs to YAML
SELECT value_to_yaml({'name': 'John', 'age': 30});
-- Returns: name: John\nage: 30

-- Convert arrays to YAML
SELECT value_to_yaml([1, 2, 3]);
-- Returns: - 1\n- 2\n- 3

-- Convert nested structures
SELECT value_to_yaml({
    'user': {'name': 'John', 'email': 'john@example.com'},
    'scores': [85, 90, 95]
});
```

### From JSON

```sql
-- Cast JSON to YAML
SELECT '{"name": "John", "age": 30}'::JSON::YAML;
-- Returns: {name: John, age: 30}
```

## Type Conversions

### YAML to JSON

```sql
-- Using function
SELECT yaml_to_json('name: John\nage: 30'::YAML);
-- Returns: {"name":"John","age":30}

-- Using cast
SELECT 'name: John'::YAML::JSON;
```

### YAML to VARCHAR

```sql
-- Implicit in most contexts
SELECT 'name: John'::YAML;

-- Explicit cast
SELECT 'name: John'::YAML::VARCHAR;
```

### JSON to YAML

```sql
SELECT '{"key": "value"}'::JSON::YAML;
-- Returns: {key: value}
```

## YAML Formatting Styles

YAML supports two formatting styles:

### Flow Style

Compact, inline format similar to JSON:

```yaml
{name: John, age: 30, scores: [85, 90, 95]}
```

### Block Style

Multi-line format with indentation:

```yaml
name: John
age: 30
scores:
  - 85
  - 90
  - 95
```

### Controlling Format

```sql
-- Format as block style
SELECT format_yaml('name: John\nscores: [1, 2, 3]'::YAML, 'block');

-- Format as flow style
SELECT format_yaml('name: John\nscores: [1, 2, 3]'::YAML, 'flow');

-- Set default style for session
SELECT yaml_set_default_style('block');
SELECT yaml_get_default_style();  -- Returns current setting
```

## Working with YAML Columns

### Creating Tables

```sql
CREATE TABLE app_configs (
    app_name VARCHAR,
    config YAML,
    created_at TIMESTAMP DEFAULT current_timestamp
);
```

### Inserting Data

```sql
-- Insert YAML strings
INSERT INTO app_configs (app_name, config) VALUES
    ('web', 'port: 8080
workers: 4
ssl: true'),
    ('api', 'port: 3000
rate_limit: 100
cors: true');

-- Insert from DuckDB values
INSERT INTO app_configs (app_name, config) VALUES
    ('cache', value_to_yaml({'host': 'localhost', 'port': 6379}));
```

### Querying Data

```sql
-- Extract specific values
SELECT
    app_name,
    yaml_extract(config, '$.port') AS port,
    yaml_extract_string(config, '$.ssl') AS ssl_enabled
FROM app_configs;

-- Filter based on YAML content
SELECT * FROM app_configs
WHERE yaml_extract(config, '$.port')::INTEGER > 5000;

-- Check for field existence
SELECT * FROM app_configs
WHERE yaml_exists(config, '$.rate_limit');
```

## Multi-Document YAML

YAML supports multiple documents in a single value:

```sql
-- Create multi-document YAML
SELECT '---
name: Doc1
---
name: Doc2'::YAML;
```

When converting multi-document YAML to JSON:

```sql
SELECT yaml_to_json('---
name: Doc1
---
name: Doc2'::YAML);
-- Returns: [{"name":"Doc1"},{"name":"Doc2"}]
```

## Validation

Check if a string is valid YAML:

```sql
-- Valid YAML
SELECT yaml_valid('name: John');  -- true

-- Invalid YAML
SELECT yaml_valid('name: : invalid: yaml');  -- false

-- NULL handling
SELECT yaml_valid(NULL);  -- NULL
```

## Examples

### Configuration Management

```sql
-- Store application configurations
CREATE TABLE service_configs AS
SELECT * FROM (VALUES
    ('auth', 'jwt:
  secret: ${JWT_SECRET}
  expiry: 3600
oauth:
  enabled: true
  providers: [google, github]'),
    ('database', 'primary:
  host: db-primary.local
  port: 5432
replica:
  host: db-replica.local
  port: 5432
  read_only: true')
) AS t(service, config);

-- Query nested configuration
SELECT
    service,
    yaml_extract_string(config::YAML, '$.primary.host') AS primary_host,
    yaml_extract(config::YAML, '$.oauth.providers') AS oauth_providers
FROM service_configs;
```

### Data Transformation

```sql
-- Convert table to YAML format
SELECT value_to_yaml(struct_pack(
    name := name,
    email := email,
    department := department
)) AS yaml_record
FROM employees;
```

### Template Storage

```sql
-- Store YAML templates
CREATE TABLE email_templates (
    template_name VARCHAR PRIMARY KEY,
    template YAML
);

INSERT INTO email_templates VALUES
('welcome', 'subject: Welcome to {{company}}!
body: |
  Dear {{name}},

  Welcome to our platform.

  Best regards,
  The Team
variables:
  - company
  - name');

-- Query template variables
SELECT
    template_name,
    yaml_extract(template, '$.variables') AS required_vars
FROM email_templates;
```

## Best Practices

### When to Use YAML Type

Use the YAML type when you need to:

- Store configuration data
- Preserve YAML-specific formatting
- Use YAML extraction functions
- Convert between YAML and other formats

### When to Use JSON Instead

Consider JSON when:

- You need JSON-specific functions
- Interoperability with JSON APIs is required
- The data originated as JSON

### Performance Considerations

- YAML is stored as VARCHAR internally
- Parsing happens on each function call
- For frequently accessed data, consider extracting to native columns
- Use `columns` parameter in `read_yaml` to convert at load time

## See Also

- [Conversion Functions](../functions/conversion.md) - Converting between types
- [Extraction Functions](../functions/extraction.md) - Querying YAML values
- [Writing YAML Files](writing-yaml.md) - Exporting to YAML format
