# Table Functions Reference

Complete reference for YAML table functions.

## read_yaml

Reads YAML files into a table.

### Signature

```sql
read_yaml(path VARCHAR, [options...]) → TABLE
```

### Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `path` | VARCHAR or VARCHAR[] | Required | File path, glob pattern, or list of files |
| `auto_detect` | BOOLEAN | `true` | Enable automatic type detection |
| `columns` | STRUCT | - | Explicit column type specification |
| `records` | VARCHAR | - | Path to nested array of records (dot notation) |
| `multi_document` | BOOLEAN | `true` | Handle multiple YAML documents |
| `expand_root_sequence` | BOOLEAN | `true` | Expand top-level sequences into rows |
| `ignore_errors` | BOOLEAN | `false` | Continue on parsing errors |
| `maximum_object_size` | INTEGER | 16777216 | Maximum file size in bytes |
| `sample_size` | INTEGER | 20480 | Rows to sample for schema detection |
| `maximum_sample_files` | INTEGER | 32 | Files to sample for schema detection |

### Returns

TABLE with columns inferred from YAML structure or specified via `columns` parameter.

### Examples

```sql
-- Basic usage
SELECT * FROM read_yaml('config.yaml');

-- With glob pattern
SELECT * FROM read_yaml('data/*.yaml');

-- With file list
SELECT * FROM read_yaml(['file1.yaml', 'file2.yaml']);

-- With parameters
SELECT * FROM read_yaml('data.yaml',
    auto_detect = true,
    ignore_errors = true,
    columns = {'id': 'INTEGER', 'name': 'VARCHAR'}
);

-- Extract nested records
SELECT * FROM read_yaml('config.yaml', records = 'data.users');

-- Deeply nested path
SELECT * FROM read_yaml('app.yaml', records = 'config.database.tables');
```

---

## read_yaml_objects

Reads YAML files preserving document structure.

### Signature

```sql
read_yaml_objects(path VARCHAR, [options...]) → TABLE
```

### Parameters

Same as `read_yaml`, plus:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `filename` | BOOLEAN | `false` | Include source filename column |

### Returns

TABLE with one row per file/document, preserving nested structure.

### Examples

```sql
-- Read with filename
SELECT * FROM read_yaml_objects('configs/*.yaml', filename = true);

-- Access nested data
SELECT
    filename,
    yaml_extract_string(data, '$.name') AS name
FROM read_yaml_objects('data/*.yaml', filename = true);
```

---

## read_yaml_frontmatter

Extracts YAML frontmatter from text files.

### Signature

```sql
read_yaml_frontmatter(path VARCHAR, [options...]) → TABLE
```

### Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `path` | VARCHAR or VARCHAR[] | Required | File path or glob pattern |
| `as_yaml_objects` | BOOLEAN | `false` | Return raw YAML column |
| `content` | BOOLEAN | `false` | Include file body |
| `filename` | BOOLEAN | `false` | Include source filename |

### Returns

TABLE with columns from frontmatter fields, plus optional `content` and `filename` columns.

### Examples

```sql
-- Basic frontmatter extraction
SELECT * FROM read_yaml_frontmatter('posts/*.md');

-- With all options
SELECT * FROM read_yaml_frontmatter('docs/**/*.md',
    content = true,
    filename = true
);

-- Raw YAML mode
SELECT frontmatter FROM read_yaml_frontmatter('posts/*.md',
    as_yaml_objects = true
);
```

---

## parse_yaml

Parses a YAML string into a table.

### Signature

```sql
parse_yaml(yaml_string VARCHAR) → TABLE
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_string` | VARCHAR | YAML content to parse |

### Returns

TABLE with columns inferred from YAML structure.

### Examples

```sql
-- Parse single document
SELECT * FROM parse_yaml('name: John
age: 30');

-- Parse multi-document
SELECT * FROM parse_yaml('---
name: Alice
---
name: Bob');

-- Parse sequence
SELECT * FROM parse_yaml('- item: one
- item: two');
```

---

## yaml_each

Returns key-value pairs from a YAML object.

### Signature

```sql
yaml_each(yaml_value YAML) → TABLE(key VARCHAR, value YAML)
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_value` | YAML | A YAML object |

### Returns

TABLE with columns:

- `key` VARCHAR - Key name
- `value` YAML - Associated value

### Examples

```sql
-- Iterate object
SELECT * FROM yaml_each('{a: 1, b: 2, c: 3}'::YAML);
-- Returns: (a, 1), (b, 2), (c, 3)

-- With value processing
SELECT key, yaml_type(value) AS type
FROM yaml_each(config);
```

---

## yaml_array_elements

Unnests YAML array elements into rows.

### Signature

```sql
yaml_array_elements(yaml_value YAML) → TABLE(value YAML)
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `yaml_value` | YAML | A YAML array |

### Returns

TABLE with column:

- `value` YAML - Each array element

### Examples

```sql
-- Unnest array
SELECT * FROM yaml_array_elements('[1, 2, 3]'::YAML);
-- Returns: 1, 2, 3

-- Process elements
SELECT yaml_extract_string(value, '$.name') AS name
FROM yaml_array_elements('[{name: A}, {name: B}]'::YAML);
```

---

## COPY TO (YAML Format)

Exports data to YAML files.

### Signature

```sql
COPY (query) TO 'path' (FORMAT yaml [, STYLE style] [, LAYOUT layout])
```

### Parameters

| Parameter | Values | Default | Description |
|-----------|--------|---------|-------------|
| `FORMAT` | `yaml` | Required | Output format |
| `STYLE` | `flow`, `block` | `flow` | YAML formatting style |
| `LAYOUT` | `document`, `sequence` | `document` | Row organization |

### Examples

```sql
-- Basic export
COPY (SELECT * FROM users) TO 'users.yaml' (FORMAT yaml);

-- Block style with sequence layout
COPY (SELECT * FROM data) TO 'output.yaml'
(FORMAT yaml, STYLE block, LAYOUT sequence);
```

---

## Direct File Access

YAML files can be queried directly:

```sql
-- These are equivalent:
SELECT * FROM 'data.yaml';
SELECT * FROM read_yaml('data.yaml');
```

Supported extensions: `.yaml`, `.yml`
