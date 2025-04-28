# YAML Extension for DuckDB

This extension allows DuckDB to read YAML files directly into tables. It provides a simple interface for accessing YAML data within SQL queries.

## Meta Considerations

At a more meta level, this extension is an exercise in having Claude.ai create an extension for me, using other extensions as examples, while I try to delay diving into the code myself for as long as possible. The inital chat I used to start this project is shared here. Eventually, I created a project with integrations into this GitHub repository, which prevents additional sharing, but I will add the chats to this repository for reference.

## Features

- Read YAML files into DuckDB tables with `read_yaml`
- Preserve document structure with `read_yaml_objects`
- Auto-detect data types from YAML content
- Support for multi-document YAML files
- Handle top-level sequences as rows
- Robust parameter handling and error recovery

## Usage

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

-- Read from a file (creates a row per document or sequence item)
SELECT * FROM read_yaml('path/to/file.yaml');

-- Read preserving document structure (one row per file)
SELECT * FROM read_yaml_objects('path/to/file.yaml');
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

### Error Handling

The YAML extension provides robust error handling capabilities:

 * Use `ignore_errors=true` to continue processing despite errors in some files
 * With `ignore_errors=true` and multi-document YAML, valid documents are still processed even if some have errors
 * When providing a file list, parsing continues with the next file if an error occurs in one file

### Accessing YAML Data

The reader converts YAML structures to native DuckDB types:

```sql
-- Access scalar values (using DuckDB's dot notation)
SELECT name, age FROM yaml_table;

-- Access array elements (1-based indexing)
SELECT tags[1], tags[2] FROM yaml_table;

-- Access nested structures
SELECT person.addresses[1].city FROM yaml_table;
```

### Multi-document and Sequence Handling

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

Both formats will produce the same result when read with `read_yaml()`:

```sql
SELECT id, name FROM read_yaml('file.yaml');
```

Result:
```
┌─────┬──────┐
│ id  │ name │
├─────┼──────┤
│ 1   │ John │
│ 2   │ Jane │
└─────┴──────┘
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

## Limitations and Future Work

Current limitations:
- No support for YAML anchors and aliases
- No direct YAML to JSON conversion
- Limited type detection
- No streaming for large files

Planned features:
- YAML type system and conversion functions
- Improved type detection for dates, timestamps, etc.
- Support for YAML-specific features
- Path expressions and extraction functions
- Streaming for large files

## License

This extension is released under the MIT License.
