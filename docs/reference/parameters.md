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
| `maximum_object_size` | INTEGER | `16777216` | Max file size (16MB) |

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

---

## maximum_object_size

Maximum allowed file size in bytes.

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
| `flow` (default) | Compact inline format |
| `block` | Multi-line indented format |

### LAYOUT

| Value | Description |
|-------|-------------|
| `document` (default) | Each row as YAML document |
| `sequence` | All rows as sequence items |

**Example:**

```sql
COPY (SELECT * FROM data) TO 'output.yaml'
(FORMAT yaml, STYLE block, LAYOUT sequence);
```
