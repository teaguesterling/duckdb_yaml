# Parameters Reference

Complete reference for all configurable parameters.

## read_yaml Parameters

### Input Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `path` | VARCHAR or VARCHAR[] | Required | File path, glob pattern, or list of paths |

### Schema Detection

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `auto_detect` | BOOLEAN | `true` | Enable automatic type detection |
| `columns` | STRUCT | - | Explicit column types |
| `sample_size` | INTEGER | `20480` | Rows to sample for schema detection |
| `maximum_sample_files` | INTEGER | `32` | Files to sample for schema detection |

### Data Extraction

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `records` | VARCHAR | - | Path to nested array of records (dot notation) |

### Parsing Behavior

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `multi_document` | BOOLEAN | `true` | Handle multiple YAML documents |
| `expand_root_sequence` | BOOLEAN | `true` | Expand sequences into rows |
| `ignore_errors` | BOOLEAN | `false` | Continue on errors |
| `compression` | VARCHAR | `'auto'` | `auto`, `gzip`, `zstd` or `none` |
| `maximum_object_size` | INTEGER | `16777216` | Max size of the text of a file (16MB) |

---

## auto_detect

Controls automatic type detection.

**Values:**

- `true` (default): Detect types (DATE, TIME, TIMESTAMP, numeric types, BOOLEAN)
- `false`: All columns become VARCHAR

**Example:**

```sql
-- Enable (default)
SELECT * FROM read_yaml('data.yaml', auto_detect = true);

-- Disable
SELECT * FROM read_yaml('data.yaml', auto_detect = false);
```

---

## columns

Specifies explicit column types.

**Value:** Struct mapping column names to type strings.

**Supported Types:**

- Basic: `VARCHAR`, `INTEGER`, `BIGINT`, `DOUBLE`, `BOOLEAN`, `DATE`, `TIMESTAMP`
- Complex: `STRUCT(...)`, `VARCHAR[]`, `INTEGER[]`, `MAP(...)`, `JSON`, `YAML`

**Example:**

```sql
SELECT * FROM read_yaml('data.yaml', columns = {
    'id': 'BIGINT',
    'name': 'VARCHAR',
    'created': 'TIMESTAMP',
    'tags': 'VARCHAR[]',
    'metadata': 'JSON'
});
```

**Interaction with auto_detect:**

- Specified columns use explicit types
- Unspecified columns use auto_detect behavior

---

## records

Extract records from a nested array path using dot notation.

**Value:** String with dot-separated path segments.

**Example:**

```yaml
# config.yaml
metadata:
  version: "1.0"
projects:
  - name: alpha
    status: active
  - name: beta
    status: inactive
```

```sql
-- Extract the nested projects array
SELECT * FROM read_yaml('config.yaml', records = 'projects');
```

**Nested paths:**

```yaml
# data.yaml
config:
  database:
    tables:
      - name: users
        rows: 1000
      - name: orders
        rows: 5000
```

```sql
-- Navigate multiple levels deep
SELECT * FROM read_yaml('data.yaml', records = 'config.database.tables');
```

**Error handling:**

| Condition | Behavior |
|-----------|----------|
| Path not found | Error (or skip with `ignore_errors = true`) |
| Path points to non-array | Error (or skip with `ignore_errors = true`) |
| Array contains non-objects | Non-object elements are filtered out |

**Interaction with other parameters:**

- `expand_root_sequence` is automatically disabled when `records` is specified
- `columns` can be combined to specify types for the extracted records
- `ignore_errors` allows graceful handling of missing paths

---

## sample_size

Number of rows to sample for schema detection.

The schema step parses a file only as far as the sample, so a large file binds quickly. A syntax error past the sample is an error when you read the rows, and `DESCRIBE` does not show it. With `ignore_errors = true`, or with `multi_document` set to `first`, `frontmatter` or `list`, or with `records`, the schema step parses the whole file.

**Values:**

- Positive integer: Sample this many rows
- `-1`: Unlimited (sample all rows)

**Default:** `20480`

**Example:**

```sql
-- Sample more rows for complex schemas
SELECT * FROM read_yaml('data/*.yaml', sample_size = 50000);

-- Sample all rows
SELECT * FROM read_yaml('data/*.yaml', sample_size = -1);
```

---

## maximum_sample_files

Maximum files to sample for schema detection.

**Values:**

- Positive integer: Sample up to this many files
- `-1`: Unlimited (sample all files)

**Default:** `32`

**Example:**

```sql
-- Sample more files
SELECT * FROM read_yaml('data/*.yaml', maximum_sample_files = 100);

-- Sample all files
SELECT * FROM read_yaml('data/*.yaml', maximum_sample_files = -1);
```

---

## multi_document

Handle multiple YAML documents in a single file.

**Values:**

- `true` (default): Each document becomes a separate row
- `false`: Only read the first document

**Example:**

```yaml
# File with multiple documents
---
name: Doc1
---
name: Doc2
```

```sql
-- Returns 2 rows (default)
SELECT * FROM read_yaml('multi.yaml', multi_document = true);

-- Returns 1 row
SELECT * FROM read_yaml('multi.yaml', multi_document = false);
```

---

## expand_root_sequence

Expand top-level YAML sequences into rows.

**Values:**

- `true` (default): Each sequence item becomes a row
- `false`: Keep sequence as single value

**Example:**

```yaml
# Sequence file
- name: Alice
- name: Bob
```

```sql
-- Returns 2 rows (default)
SELECT * FROM read_yaml('list.yaml', expand_root_sequence = true);

-- Returns 1 row with array
SELECT * FROM read_yaml('list.yaml', expand_root_sequence = false);
```

---

## ignore_errors

Continue processing when errors occur.

**Values:**

- `false` (default): Stop on first error
- `true`: Skip invalid files/documents

**Example:**

```sql
-- Skip bad files
SELECT * FROM read_yaml('data/*.yaml', ignore_errors = true);
```

**Behavior:**

| Error Type | ignore_errors=true |
|------------|-------------------|
| File not found | Skip file |
| Permission denied | Skip file |
| Invalid YAML | Skip document |
| Type conversion | Value becomes NULL |

With `ignore_errors=false`, a value that the column type cannot hold is an error. A `null` in the file and a missing key are NULL in both cases.

---

## compression

How to decompress the file. `auto` uses the extension: `.gz` is gzip and `.zst` is zstd.

**Default:** `'auto'`

**Example:**

```sql
SELECT * FROM read_yaml('data.yaml.gz');
SELECT * FROM 'data.yaml.gz';

-- The name of the file does not give the compression
SELECT * FROM read_yaml('data.backup', compression = 'gzip');
```

DuckDB reads zstd through the `parquet` extension, so that extension must be available. `read_yaml_frontmatter` always uses `auto`.

---
## maximum_object_size

Maximum allowed size of the text of a file, in bytes. For a compressed file, the limit applies to the decompressed text.

**Default:** `16777216` (16MB)

**Example:**

```sql
-- Allow larger files (64MB)
SELECT * FROM read_yaml('large.yaml', maximum_object_size = 67108864);
```

---

## read_yaml_frontmatter Parameters

### Input Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `path` | VARCHAR or VARCHAR[] | Required | File path or glob pattern |

### Output Control

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `as_yaml_objects` | BOOLEAN | `false` | Return raw YAML column |
| `content` | BOOLEAN | `false` | Include file body |
| `filename` | BOOLEAN | `false` | Include source filename |

---

## as_yaml_objects

Return raw YAML instead of expanded columns.

**Values:**

- `false` (default): Expand frontmatter fields as columns
- `true`: Return single `frontmatter` column of YAML type

**Example:**

```sql
-- Expanded columns (default)
SELECT title, author, date FROM read_yaml_frontmatter('posts/*.md');

-- Raw YAML
SELECT frontmatter FROM read_yaml_frontmatter('posts/*.md',
    as_yaml_objects = true);
```

---

## content

Include file content after frontmatter.

**Values:**

- `false` (default): Exclude body content
- `true`: Include as `content` column

**Example:**

```sql
SELECT title, content FROM read_yaml_frontmatter('posts/*.md',
    content = true);
```

---

## filename

Include source file path.

**Values:**

- `false` (default): Exclude filename
- `true`: Include as `filename` column

**Example:**

```sql
SELECT filename, title FROM read_yaml_frontmatter('posts/*.md',
    filename = true);
```

---

## COPY TO Parameters

### FORMAT

| Value | Description |
|-------|-------------|
| `yaml` | YAML output format |

### STYLE

| Value | Description |
|-------|-------------|
| `block` (default) | Multi-line indented format |
| `flow` | Compact inline format |

### LAYOUT

| Value | Description |
|-------|-------------|
| `document` (default) | Each row as YAML document |
| `sequence` | All rows as sequence items |

### MULTILINE

| Value | Description |
|-------|-------------|
| `auto` (default) | Resolves to `literal` for block style, `quoted` for flow |
| `literal` | Use YAML literal block scalars (`\|` or `\|-`) for multiline strings. A string that a block cannot hold exactly, such as one with leading whitespace, is quoted |
| `quoted` | Use quoted strings with escape sequences |

### INDENT

| Value | Description |
|-------|-------------|
| `1`-`10` (default: `2`) | Indentation width for block style output |

### DOCUMENT_SEPARATOR

| Value | Description |
|-------|-------------|
| `true` | Write `---` between documents. This is the default for `STYLE block`, where it cannot be turned off, because the rows would merge |
| `false` | Write one document per line, with no `---`. This is the default for `STYLE flow` |

With `STYLE flow`, the default output is not valid multi-document YAML if it has more than one row. `read_yaml` reads it, but other parsers, such as PyYAML, reject it. Set `DOCUMENT_SEPARATOR true` for those parsers. Requires `LAYOUT document`.

### EXPLICIT_START

| Value | Description |
|-------|-------------|
| `false` (default) | Each file starts with its first document. A `---` separates each pair of documents |
| `true` | Also write `---` before the first document of each file |

Requires output with separators: `LAYOUT document`, and `STYLE block` or `DOCUMENT_SEPARATOR true`. Any other combination is an error.

**Example:**

```sql
COPY (SELECT * FROM data) TO 'output.yaml'
(FORMAT yaml, STYLE block, LAYOUT sequence, MULTILINE literal, INDENT 4);
```
