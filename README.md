# YAML Extension for DuckDB

This extension allows DuckDB to read YAML files directly into tables and provides full YAML type support with conversion functions. It enables seamless integration of YAML data within SQL queries.

## AI-written Extension
Claude.ai wrote 99% of the code in this project over the course of a weekend.

## Features

- Read YAML files into DuckDB tables with `read_yaml`
- Preserve document structure with `read_yaml_objects`
- Auto-detect data types from YAML content
- Support for multi-document YAML files
- Handle top-level sequences as rows
- Convert between YAML, JSON, and other types
- Robust parameter handling and error recovery
- Direct file path support for easy querying
- Write query results to YAML files with `COPY TO`

## Usage

### Quickstart
```sql
-- Load extension
LOAD yaml;

-- Query YAML file directly
FROM 'data/*.yml'

-- Create YAML type
CREATE TABLE configs(id INTEGER, config YAML);

-- Convert between YAML and JSON
SELECT yaml_to_json(config) FROM configs;
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
--   count: 42
--   active: true
--   score: 3.14
-- Results in columns: date DATE, count TINYINT, active BOOLEAN, score DOUBLE
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
```

### YAML Type and Conversions

The extension provides a YAML type for storing and manipulating YAML data:

```sql
-- Create table with YAML column
CREATE TABLE configs(id INTEGER, config YAML);

-- Insert YAML values
INSERT INTO configs VALUES (1, 'server: production
port: 8080
features: 
  - logging
  - metrics');

-- Convert YAML to JSON
SELECT id, yaml_to_json(config) AS config_json FROM configs;

-- Convert JSON to YAML
SELECT json_to_yaml('{"name":"John","age":30}') AS yaml_config;

-- Convert any value to YAML
SELECT value_to_yaml([1, 2, 3]) AS yaml_array;

-- Cast between types
SELECT config::JSON FROM configs;
SELECT '{"name":"John"}'::JSON::YAML;
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

 * Use `ignore_errors=true` to continue processing despite errors in some files
 * With `ignore_errors=true` and multi-document YAML, valid documents are still processed even if some have errors
 * When providing a file list, parsing continues with the next file if an error occurs in one file

### Accessing YAML Data

Access YAML data using standard SQL operations:

```sql
-- Access values via JSON conversion
SELECT json_extract_string(yaml_to_json(config), '$.server') AS server FROM configs;

-- Filter based on YAML content
SELECT * FROM configs 
WHERE json_extract_string(yaml_to_json(config), '$.environment') = 'production';

-- Join with YAML data
SELECT c.id, json_extract_int(yaml_to_json(c.config), '$.port') AS port
FROM configs c
JOIN environments e ON json_extract_string(yaml_to_json(c.config), '$.environment') = e.name;
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

The YAML extension now includes comprehensive automatic type detection:

- **Temporal Types**: Automatically detects DATE, TIMESTAMP, and TIME values
- **Numeric Types**: Intelligently chooses between TINYINT, SMALLINT, INTEGER, BIGINT, and DOUBLE based on value ranges
- **Boolean Values**: Case-insensitive detection of various formats (true/false, yes/no, on/off, y/n, t/f)
- **Special Values**: Handles infinity (inf, -inf) and NaN values
- **Null Values**: Recognizes null, ~, and empty strings as NULL
- **Mixed Types**: Arrays with mixed types automatically fall back to VARCHAR[]

## Known Limitations

Current limitations:
- No streaming support for large files yet
- Path expressions not yet implemented
- Explicit column type specification not yet implemented

## Planned Features

- YAML path expressions and extraction functions
- Streaming support for large files
- Explicit column type specification via 'columns' parameter
- YAML modification functions
- Support for YAML anchors and aliases

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
