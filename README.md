[![DuckDB Community Extension](https://img.shields.io/badge/yaml-DuckDB_Community_Extension-blue?logo=duckdb)](https://duckdb.org/community_extensions/extensions/yaml.html)

# YAML Extension for DuckDB

This extension allows DuckDB to read YAML files directly into tables and provides full YAML type support with conversion functions. It enables seamless integration of YAML data within SQL queries.

## Installation

### From Community Extensions

```sql
INSTALL yaml FROM community;
LOAD yaml;
```

### From GitHub Release

```sql
-- Install directly from GitHub releases
INSTALL yaml FROM 'https://github.com/teaguesterling/duckdb_yaml/releases/download/v0.1.0/yaml.duckdb_extension';
LOAD yaml;
```

### From Source

```bash
git clone https://github.com/teaguesterling/duckdb_yaml
cd duckdb_yaml
make

# To run tests
make test
```

## AI-written Extension
Claude.ai wrote 99% of the code in this project as an experiment. The original working version was written over the course of a weekend and refined periodically to until a production-ready state was reached.

## Features

- **YAML File Reading**: Read YAML files into DuckDB tables with `read_yaml` and `read_yaml_objects`
- **Frontmatter Extraction**: Extract YAML frontmatter from Markdown, MDX, Astro, RST, and other text files
- **Native YAML Type**: Full YAML type support with seamless conversion between YAML, JSON, and VARCHAR
- **Type Detection**: Comprehensive automatic type detection including temporal types (DATE, TIME, TIMESTAMP) and optimal numeric types
- **YAML Extraction**: Query YAML data with extraction functions (`yaml_extract`, `yaml_type`, `yaml_exists`)
- **Column Type Specification**: Explicitly define column types when reading YAML files
- **Multi-Document Support**: Handle files with multiple YAML documents separated by `---`
- **Sequence Handling**: Automatically expand top-level sequences into rows
- **YAML Output**: Write query results to YAML files with `COPY TO` in various formats
- **Direct File Querying**: Query YAML files directly using `FROM 'file.yaml'` syntax
- **Error Recovery**: Continue processing valid documents even when some have errors

## Usage

### Quick Start

```sql
-- Load the extension
LOAD yaml;

-- Query YAML files directly
SELECT * FROM 'data/config.yaml';
SELECT * FROM 'data/*.yml' WHERE active = true;

-- Create a table with YAML column
CREATE TABLE configs(id INTEGER, config YAML);

-- Insert YAML data
INSERT INTO configs VALUES 
    (1, 'environment: prod\nport: 8080'),
    (2, '{environment: dev, port: 3000}');

-- Query YAML data
SELECT 
    id,
    yaml_extract_string(config, '$.environment') AS env,
    yaml_extract(config, '$.port') AS port
FROM configs;

-- Convert YAML to JSON for complex queries
SELECT yaml_to_json(config) AS json_config FROM configs;
```

### Loading the Extension

```sql
LOAD yaml;
```

### Reading YAML Files

#### Basic Usage

```sql
-- Single file path
SELECT * FROM read_yaml('data/config.yaml');

-- Glob pattern (uses filesystem glob matching)
SELECT * FROM read_yaml('data/*.yaml');

-- List of files (processes all files in the array)
SELECT * FROM read_yaml(['config1.yaml', 'config2.yaml', 'data/settings.yaml']);

-- Directory path (reads all .yaml and .yml files in directory)
SELECT * FROM read_yaml('data/configs/');

-- Read preserving document structure (one row per file)
SELECT * FROM read_yaml_objects('path/to/file.yaml');

-- Type detection example
CREATE TABLE events AS 
SELECT * FROM read_yaml('events.yaml');
-- Given YAML:
-- - date: 2024-01-15
--   time: 14:30:00
--   timestamp: 2024-01-15T14:30:00Z
--   count: 42
--   active: true
--   score: 3.14
-- Results in columns: date DATE, time TIME, timestamp TIMESTAMP, count TINYINT, active BOOLEAN, score DOUBLE
```

#### With Parameters

```sql
-- Auto-detect types (default: true)
SELECT * FROM read_yaml('file.yaml', auto_detect=true);

-- Handle multiple YAML documents (default: true)
SELECT * FROM read_yaml('file.yaml', multi_document=true);

-- Expand top-level sequences into rows (default: true)
SELECT * FROM read_yaml('file.yaml', expand_root_sequence=true);

-- Ignore parsing errors (default: false)
SELECT * FROM read_yaml('file.yaml', ignore_errors=true);

-- Set maximum file size in bytes (default: 16MB)
SELECT * FROM read_yaml('file.yaml', maximum_object_size=1048576);

-- Specify explicit column types
SELECT * FROM read_yaml('file.yaml', columns={
    'id': 'INTEGER',
    'name': 'VARCHAR',
    'created': 'TIMESTAMP'
});
```

### Reading Frontmatter

Extract YAML frontmatter from Markdown, MDX, Astro, reStructuredText, and other text files. Frontmatter is YAML metadata enclosed between `---` delimiters at the start of a file.

```sql
-- Read frontmatter from Markdown blog posts
SELECT title, author, date, tags
FROM read_yaml_frontmatter('posts/*.md');

-- Query documentation metadata across different formats
SELECT title, sidebar_position, tags
FROM read_yaml_frontmatter('docs/**/*.{md,mdx,rst}');

-- Get raw YAML instead of expanded columns
SELECT frontmatter FROM read_yaml_frontmatter('posts/*.md', as_yaml_objects:=true);

-- Include file content after frontmatter
SELECT title, content FROM read_yaml_frontmatter('posts/*.md', content:=true);

-- Include filename for tracking source
SELECT filename, title FROM read_yaml_frontmatter('posts/*.md', filename:=true);
```

#### Supported File Types

Frontmatter works with any text file that uses `---` delimiters:

| Format | Extension | Common Use |
|--------|-----------|------------|
| Markdown | `.md` | Blog posts, documentation |
| MDX | `.mdx` | Docusaurus, interactive docs |
| Astro | `.astro` | Astro components |
| reStructuredText | `.rst` | Python documentation |
| Nunjucks | `.njk` | Eleventy templates |
| Plain text | `.txt` | Notes, any text file |

#### Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `as_yaml_objects` | `false` | If `true`, return raw YAML in a single column. If `false`, expand fields as columns. |
| `content` | `false` | Include the file body (after frontmatter) as a `content` column |
| `filename` | `false` | Include a `filename` column with the source file path |
| `union_by_name` | `true` | Merge schemas when files have different frontmatter fields |

#### Example: Analyzing a Blog

```sql
-- Find all draft posts
SELECT filename, title, date
FROM read_yaml_frontmatter('content/posts/*.md', filename:=true)
WHERE draft = true;

-- Get posts by tag
SELECT title, date, tags
FROM read_yaml_frontmatter('content/posts/*.md')
WHERE list_contains(tags, 'tutorial')
ORDER BY date DESC;

-- Export blog metadata to CSV
COPY (
    SELECT title, author, date, tags
    FROM read_yaml_frontmatter('content/posts/*.md')
    ORDER BY date DESC
) TO 'blog_index.csv';
```

### YAML Type and Functions

#### YAML Type
The extension provides a native YAML type for storing and manipulating YAML data:

```sql
-- Create table with YAML column
CREATE TABLE configs(id INTEGER, config YAML);

-- Insert YAML values
INSERT INTO configs VALUES (1, 'server: production
port: 8080
features: 
  - logging
  - metrics');

-- Cast between types
SELECT config::JSON FROM configs;
SELECT '{"name":"John"}'::JSON::YAML;
```

#### Conversion Functions

```sql
-- Convert YAML to JSON
SELECT yaml_to_json(config) AS config_json FROM configs;

-- Convert any value to YAML
SELECT value_to_yaml({name: 'John', age: 30}) AS yaml_struct;
SELECT value_to_yaml([1, 2, 3]) AS yaml_array;

-- Convert JSON to YAML using cast
SELECT '{"name":"John","age":30}'::JSON::YAML AS yaml_data;

-- Format YAML with specific style
SELECT format_yaml(config, 'block') FROM configs;  -- block style
SELECT format_yaml(config, 'flow') FROM configs;   -- flow style (inline)

-- Validate YAML
SELECT yaml_valid('invalid: yaml: data');  -- returns false
SELECT yaml_valid(config) FROM configs;    -- returns true for YAML type
```

#### Extraction Functions

```sql
-- Extract data from YAML using path expressions
SELECT yaml_extract(config, '$.server') AS server FROM configs;
SELECT yaml_extract_string(config, '$.features[0]') AS first_feature FROM configs;

-- Check data types and existence
SELECT yaml_type(config) AS root_type FROM configs;  -- returns 'object'
SELECT yaml_type(config, '$.features') AS features_type FROM configs;  -- returns 'array'
SELECT yaml_exists(config, '$.port') AS has_port FROM configs;  -- returns true
```

#### Style Management

```sql
-- Set default YAML output style
SELECT yaml_set_default_style('block');  -- Use block style by default
SELECT yaml_set_default_style('flow');   -- Use flow style by default

-- Check current default style
SELECT yaml_get_default_style();  -- returns current setting
```

### Direct File Path Support

The YAML extension supports directly using file paths in the `FROM` clause:

```sql
-- These are equivalent:
SELECT * FROM 'data.yaml';
SELECT * FROM read_yaml('data.yaml');

-- The extension supports both .yaml and .yml file extensions
SELECT * FROM 'config.yml';

-- Glob patterns work the same way
SELECT * FROM 'data/*.yaml';

-- You can use all DuckDB SQL capabilities
SELECT name, age FROM 'people.yaml' WHERE age > 30;

-- Create tables directly
CREATE TABLE config AS SELECT * FROM 'config.yaml';
```

### Writing YAML Files

The YAML extension supports writing query results to YAML files using the `COPY TO` statement:

```sql
-- Basic COPY TO with default settings (flow style)
COPY (SELECT * FROM table) TO 'output.yaml' (FORMAT yaml);

-- Write with block style (more readable for complex data)
COPY (SELECT * FROM table) TO 'output.yaml' (FORMAT yaml, STYLE block);

-- Write as sequence (each row as a YAML sequence item)
COPY (SELECT * FROM table) TO 'output.yaml' (FORMAT yaml, LAYOUT sequence);

-- Write as documents (each row as a separate YAML document)
COPY (SELECT * FROM table) TO 'output.yaml' (FORMAT yaml, LAYOUT document, STYLE block);

-- Combine with queries and transformations
COPY (
    SELECT id, name, struct_pack(age, city) AS details 
    FROM people 
    WHERE age > 25
) TO 'adults.yaml' (FORMAT yaml, STYLE block);
```

#### COPY TO Parameters

- `STYLE`: Controls YAML formatting style
  - `flow` (default): Inline format `{id: 1, name: John}`
  - `block`: Multi-line format with proper indentation
  
- `LAYOUT`: Controls how multiple rows are formatted
  - `document` (default): Each row as a separate YAML object/document
  - `sequence`: All rows as items in a YAML sequence (array)

When using `LAYOUT document` with `STYLE block`, rows are separated by `---` (YAML document separator).

### Error Handling

The YAML extension provides robust error handling capabilities:

```sql
-- Continue processing despite errors in some files
SELECT * FROM read_yaml(['good.yaml', 'bad.yaml', 'also_good.yaml'], 
    ignore_errors=true);

-- Process valid documents even if some are malformed
SELECT * FROM read_yaml('mixed_valid_invalid.yaml', 
    multi_document=true, 
    ignore_errors=true);

-- Check if YAML strings are valid
SELECT 
    yaml_string,
    yaml_valid(yaml_string) AS is_valid
FROM raw_yaml_data;
```

Error handling behavior:
- With `ignore_errors=false` (default): Stops on first error
- With `ignore_errors=true`: Skips invalid files/documents and continues
- Invalid documents in multi-document files are skipped individually
- File access errors are reported but processing continues with remaining files

### Accessing YAML Data

You can access YAML data using the extraction functions or by converting to JSON:

```sql
-- Using YAML extraction functions (recommended)
SELECT yaml_extract_string(config, '$.server') AS server FROM configs;
SELECT yaml_extract(config, '$.features[0]') AS first_feature FROM configs;

-- Using JSON conversion for complex queries
SELECT json_extract_string(yaml_to_json(config), '$.server') AS server FROM configs;

-- Filter based on YAML content
SELECT * FROM configs 
WHERE yaml_extract_string(config, '$.environment') = 'production';

-- Join with YAML data
SELECT c.id, yaml_extract(c.config, '$.port') AS port
FROM configs c
JOIN environments e ON yaml_extract_string(c.config, '$.environment') = e.name;
```

### Column Type Specification

You can specify explicit column types when reading YAML files:

```sql
-- Basic type specification
SELECT * FROM read_yaml('data.yaml', columns={
    'id': 'INTEGER',
    'name': 'VARCHAR',
    'price': 'DECIMAL(10,2)',
    'created_at': 'TIMESTAMP'
});

-- Complex nested types
SELECT * FROM read_yaml('config.yaml', columns={
    'server': 'STRUCT(host VARCHAR, port INTEGER)',
    'features': 'VARCHAR[]',
    'settings': 'MAP(VARCHAR, VARCHAR)'
});

-- Mix explicit types with auto-detection
SELECT * FROM read_yaml('mixed.yaml', 
    auto_detect=true,
    columns={'id': 'INTEGER'}  -- Only 'id' uses explicit type
);

-- Specify YAML or JSON columns to preserve structure
SELECT * FROM read_yaml('nested.yaml', columns={
    'config': 'YAML',      -- Keep as YAML for later processing
    'metadata': 'JSON'     -- Convert to JSON type
});
```

## Multi-document and Sequence Handling

YAML files can contain multiple documents separated by `---` or top-level sequences. Both are handled automatically:

```yaml
# Multi-document example
---
id: 1
name: John
---
id: 2
name: Jane

# Sequence example
- id: 1
  name: John
- id: 2
  name: Jane
```

Both formats produce the same result when read with `read_yaml()`.

## Type Detection

The YAML extension includes comprehensive automatic type detection:

### Temporal Types
- **DATE**: ISO format (2024-01-15), slash format (01/15/2024), dot format (15.01.2024)
- **TIME**: Standard format (14:30:00), with milliseconds (14:30:00.123)
- **TIMESTAMP**: ISO 8601 with timezone (2024-01-15T14:30:00Z), without timezone

### Numeric Types
- **TINYINT**: -128 to 127
- **SMALLINT**: -32,768 to 32,767
- **INTEGER**: -2,147,483,648 to 2,147,483,647
- **BIGINT**: Larger integers
- **DOUBLE**: Decimals, scientific notation (1.23e-4), special values (inf, -inf, nan)

### Other Types
- **BOOLEAN**: true/false, yes/no, on/off, y/n, t/f (case-insensitive)
- **NULL**: null, ~, Null, NULL, empty string
- **VARCHAR**: Default for unrecognized patterns

### Array Handling
- Homogeneous arrays: `INTEGER[]`, `VARCHAR[]`, etc.
- Mixed-type arrays: Fall back to `VARCHAR[]`
- Empty arrays: Default to `VARCHAR[]`

## Known Limitations

- YAML anchors and aliases are resolved during parsing but not preserved
- Streaming support for very large files not yet implemented
- YAML comments are not preserved
- Maximum file size limited by memory (default 16MB, configurable)

## Advanced YAML Functions

### Query and Analysis Functions

#### Array and Object Analysis
```sql
-- Get the length of a YAML array
SELECT yaml_array_length('[1, 2, 3, 4, 5]'::YAML);  -- Returns: 5

-- Get array length from nested path
SELECT yaml_array_length('{items: [a, b, c]}'::YAML, '$.items');  -- Returns: 3

-- Get all keys from a YAML object
SELECT yaml_keys('{name: John, age: 30, city: NYC}'::YAML);  -- Returns: [name, age, city]

-- Get keys from nested object
SELECT yaml_keys('{person: {id: 1, email: test@example.com}}'::YAML, '$.person');  -- Returns: [id, email]
```

#### Unnesting Functions (Table Functions)
```sql
-- Unnest array elements into rows
SELECT * FROM yaml_array_elements('[1, 2, 3]'::YAML);
-- Returns:
-- 1
-- 2
-- 3

-- Unnest array of objects
SELECT * FROM yaml_array_elements('[{name: Alice}, {name: Bob}]'::YAML);
-- Returns:
-- {name: Alice}
-- {name: Bob}

-- Unnest object into key-value pairs
SELECT * FROM yaml_each('{name: John, age: 30, active: true}'::YAML) ORDER BY key;
-- Returns:
-- active | true
-- age    | 30
-- name   | John
```

#### Construction Functions
```sql
-- Build YAML objects from key-value pairs
SELECT yaml_build_object('name', 'John', 'age', 30, 'active', true);
-- Returns: {name: John, age: 30, active: true}

-- Build empty object
SELECT yaml_build_object();  -- Returns: {}

-- Build object with YAML values
SELECT yaml_build_object('data', '[1, 2, 3]'::YAML, 'nested', '{key: value}'::YAML);
-- Returns: {data: [1, 2, 3], nested: {key: value}}
```

#### Combined Usage Patterns
```sql
-- Get keys and use in queries
SELECT key, value
FROM yaml_each(config)
WHERE yaml_array_length(value, '$.items') > 3;

-- Unnest and aggregate
SELECT category, yaml_build_object('items', yaml_agg(item))
FROM items
GROUP BY category;

-- Filter based on array length
SELECT * FROM configs
WHERE yaml_array_length(settings, '$.features') >= 5;
```

## Future Enhancements

### Planned Functions (JSON Parity)

The extension will continue implementing additional YAML functions for complete JSON parity:

#### Extraction/Access Functions
```sql
-- Already implemented ✅
yaml_extract(yaml, path) → yaml
yaml_extract_string(yaml, path) → varchar
yaml_exists(yaml, path) → boolean
yaml_type(yaml [, path]) → varchar
yaml_array_length(yaml [, path]) → bigint  -- ✅ NEW
yaml_keys(yaml [, path]) → varchar[]  -- ✅ NEW

-- Planned ⏳
yaml_extract_path(yaml, path_elem1, path_elem2, ...) → yaml
yaml_extract_path_text(yaml, path_elem1, path_elem2, ...) → varchar
yaml_structure(yaml) → yaml  -- Returns structure with types
```

#### Transformation/Unnesting Functions
```sql
-- Already implemented ✅
yaml_array_elements(yaml) → table(yaml)  -- ✅ NEW
yaml_each(yaml) → table(key varchar, value yaml)  -- ✅ NEW

-- Planned ⏳
yaml_array_elements_text(yaml) → table(varchar)
yaml_each_text(yaml) → table(key varchar, value varchar)
yaml_object_keys(yaml) → table(varchar)
yaml_tree(yaml) → table  -- Recursive unnesting
yaml_populate_record(base anyelement, yaml) → anyelement
yaml_populate_recordset(base anyelement, yaml) → table
yaml_to_record(yaml) → record
yaml_to_recordset(yaml) → table
```

#### Construction Functions
```sql
-- Already implemented ✅
value_to_yaml(any) → yaml
yaml_build_object(...) → yaml  -- ✅ NEW

-- Planned ⏳
to_yaml(any) → yaml  -- Alias for value_to_yaml
row_to_yaml(record) → yaml
yaml_build_array(...) → yaml
yaml_object(keys varchar[], values varchar[]) → yaml
yaml_object(text[]) → yaml  -- From key-value pairs
array_to_yaml(anyarray) → yaml
```

#### Aggregate Functions
```sql
-- Planned ⏳
yaml_agg(any) → yaml  -- In progress, state management debugging
yaml_agg_strict(any) → yaml  -- Excludes NULLs
yaml_object_agg(key varchar, value any) → yaml
yaml_object_agg_strict(key varchar, value any) → yaml
yaml_merge_agg(yaml) → yaml  -- Merges multiple YAML objects
```

#### Utility/Manipulation Functions
```sql
-- Already implemented ✅
yaml_valid(varchar) → boolean
yaml_to_json(yaml) → json
format_yaml(any, style := 'flow'|'block') → varchar

-- Planned ⏳
yaml_strip_nulls(yaml) → yaml
yaml_pretty(yaml) → varchar  -- Pretty print
yaml_merge(yaml1, yaml2) → yaml
yaml_merge_patch(target yaml, patch yaml) → yaml
yaml_contains(yaml, yaml) → boolean
yaml_contained_in(yaml, yaml) → boolean
yaml_equal(yaml, yaml) → boolean
```

#### YAML-Specific Functions
```sql
yaml_aliases(yaml) → table(anchor varchar, references bigint)
yaml_merge_keys(yaml) → yaml  -- Resolve YAML merge keys (<<)
yaml_documents(yaml) → table(yaml)  -- Split multi-document YAML
yaml_combine_documents(yaml[]) → yaml  -- Combine to multi-document
yaml_matches_schema(yaml, schema) → boolean
yaml_validate_schema(schema) → boolean
```

#### Operators (if supported by DuckDB)
```sql
yaml -> path → yaml           -- Extract field/element
yaml ->> path → varchar       -- Extract as text
yaml #> path[] → yaml         -- Extract path
yaml #>> path[] → varchar     -- Extract path as text
yaml @> yaml → boolean        -- Contains
yaml <@ yaml → boolean        -- Contained by
yaml ? key → boolean          -- Key exists
yaml || yaml → yaml          -- Concatenate/merge
yaml - key → yaml            -- Delete key
```

### Implementation Priority

**Phase 1 - Core Functions** (High Priority):
- `yaml_array_length` - Essential for array operations
- `yaml_keys` - Get object keys
- `yaml_array_elements` - Unnest arrays into rows
- `yaml_each` - Iterate over key-value pairs
- `yaml_build_object` - Construct YAML objects
- `yaml_agg` - Aggregate values into YAML

**Phase 2 - Advanced Features**:
- `->` and `->>` operators for path extraction
- `yaml_merge` and `yaml_contains` for object operations
- `yaml_strip_nulls` for data cleaning
- Aggregate functions for complex data transformations

**Phase 3 - YAML-Specific Features**:
- Schema validation functions
- Multi-document handling utilities
- YAML anchor and merge key resolution

### Other Enhancements

- Streaming support for very large files
- Preserve YAML comments and formatting in round-trip operations
- Advanced path expressions with wildcards and filters
- Performance optimizations for large-scale data processing

## Examples

### Working with Configuration Files

```yaml
# config.yaml
database:
  host: localhost
  port: 5432
  credentials:
    username: admin
    password: secret
features:
  - authentication
  - logging
  - monitoring
settings:
  timeout: 30
  retries: 3
```

```sql
-- Load and query the configuration
CREATE TABLE app_config AS 
SELECT * FROM read_yaml('config.yaml');

-- Extract nested values
SELECT 
    yaml_extract_string(database, '$.host') AS db_host,
    yaml_extract(database, '$.port') AS db_port,
    yaml_extract_string(database, '$.credentials.username') AS db_user,
    yaml_extract(features, '$[0]') AS first_feature,
    yaml_extract(settings, '$.timeout') AS timeout_seconds
FROM app_config;
```

### Processing Log Files

```yaml
# logs.yaml
- timestamp: 2024-01-15T10:30:00Z
  level: INFO
  message: Application started
  metadata:
    version: 1.2.3
- timestamp: 2024-01-15T10:30:15Z
  level: ERROR
  message: Database connection failed
  metadata:
    retry_count: 3
    error_code: DB_TIMEOUT
```

```sql
-- Analyze log entries
SELECT 
    timestamp,
    level,
    message,
    yaml_extract(metadata, '$.error_code') AS error_code
FROM read_yaml('logs.yaml')
WHERE level = 'ERROR';
```

### Data Migration

```sql
-- Export DuckDB table to YAML
COPY (
    SELECT id, name, email, struct_pack(street, city, zip) AS address
    FROM users
    WHERE active = true
) TO 'active_users.yaml' (FORMAT yaml, STYLE block);

-- Import YAML data into DuckDB
CREATE TABLE imported_users AS
SELECT * FROM read_yaml('active_users.yaml', columns={
    'id': 'INTEGER',
    'name': 'VARCHAR',
    'email': 'VARCHAR',
    'address': 'STRUCT(street VARCHAR, city VARCHAR, zip VARCHAR)'
});
```

## Building from Source

This extension uses the DuckDB extension framework. To build:

```bash
make
```

## Testing

To run the test suite:

```bash
make test
```

## License

This extension is released under the MIT License.
