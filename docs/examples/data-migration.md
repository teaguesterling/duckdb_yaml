# Data Migration Examples

Examples for importing, exporting, and transforming data with YAML.

## Import Patterns

### Basic Import

```sql
-- Import YAML to table
CREATE TABLE config_data AS
SELECT * FROM read_yaml('config.yaml');

-- Import with explicit types
CREATE TABLE events AS
SELECT * FROM read_yaml('events.yaml', columns = {
    'id': 'INTEGER',
    'timestamp': 'TIMESTAMP',
    'type': 'VARCHAR',
    'data': 'JSON'
});
```

### Import Multiple Files

```sql
-- Import all YAML files in directory
CREATE TABLE all_configs AS
SELECT *
FROM read_yaml('data/*.yaml');

-- With filename tracking
CREATE TABLE configs_with_source AS
SELECT *
FROM (SELECT filename, content::YAML AS data FROM read_text('configs/*.yaml'));
```

### Incremental Import

```sql
-- Import only new files
INSERT INTO config_history
SELECT current_timestamp AS imported_at, *
FROM (SELECT filename, content::YAML AS data FROM read_text('incoming/*.yaml'))
WHERE filename NOT IN (SELECT DISTINCT source_file FROM config_history);
```

---

## Export Patterns

### Basic Export

```sql
-- Export table to YAML
COPY (SELECT * FROM users) TO 'users.yaml' (FORMAT yaml);

-- Readable format
COPY (SELECT * FROM users) TO 'users.yaml' (FORMAT yaml, STYLE block);

-- As sequence
COPY (SELECT * FROM users) TO 'users.yaml'
(FORMAT yaml, STYLE block, LAYOUT sequence);
```

### Structured Export

```sql
-- Export with nested structure
COPY (
    SELECT
        id,
        struct_pack(
            first := first_name,
            last := last_name
        ) AS name,
        struct_pack(
            street := address_street,
            city := address_city,
            zip := address_zip
        ) AS address,
        list(phone_number) AS phones
    FROM users
    LEFT JOIN user_phones USING (user_id)
    GROUP BY users.id
) TO 'users_structured.yaml' (FORMAT yaml, STYLE block);
```

### Per-Record Export

```sql
-- Export each record to separate file
-- (Use with external scripting)
SELECT
    id,
    value_to_yaml(struct_pack(
        name := name,
        email := email,
        settings := settings
    )) AS yaml_content
FROM users;
```

---

## Format Conversion

### YAML to JSON

```sql
-- Convert YAML files to JSON
COPY (
    SELECT yaml_to_json(data) AS json_data
    FROM (SELECT filename, content::YAML AS data FROM read_text('input/*.yaml'))
) TO 'output.json';
```

### JSON to YAML

```sql
-- Convert JSON to YAML
COPY (
    SELECT data::YAML AS yaml_data
    FROM read_json('input.json')
) TO 'output.yaml' (FORMAT yaml, STYLE block);
```

### CSV to YAML

```sql
-- Convert CSV to YAML
COPY (
    SELECT value_to_yaml(struct_pack(
        id := id,
        name := name,
        email := email
    ))
    FROM read_csv('users.csv')
) TO 'users.yaml' (FORMAT yaml, STYLE block, LAYOUT sequence);
```

### YAML to CSV

```sql
-- Flatten YAML to CSV
COPY (
    SELECT
        yaml_extract_string(data, '$.id') AS id,
        yaml_extract_string(data, '$.name') AS name,
        yaml_extract_string(data, '$.email') AS email
    FROM (SELECT filename, content::YAML AS data FROM read_text('users.yaml'))
) TO 'users.csv';
```

---

## Data Transformation

### Flatten Nested Data

```sql
-- Flatten nested YAML structure
SELECT
    yaml_extract_string(data, '$.id') AS id,
    yaml_extract_string(data, '$.user.name') AS user_name,
    yaml_extract_string(data, '$.user.email') AS user_email,
    yaml_extract(data, '$.user.preferences.theme') AS theme,
    yaml_extract(data, '$.user.preferences.notifications') AS notifications
FROM (SELECT filename, content::YAML AS data FROM read_text('users.yaml'));
```

### Normalize Arrays

```sql
-- Expand arrays into rows
SELECT order_id, item.product, item.quantity, item.price
FROM (SELECT order_id, unnest(items) AS item FROM read_yaml('orders.yaml'));
```

### Aggregate to YAML

```sql
-- Aggregate rows into YAML structure
SELECT value_to_yaml(struct_pack(
    department := department,
    employee_count := COUNT(*),
    employees := list(struct_pack(
        name := name,
        title := title,
        salary := salary
    ))
))
FROM employees
GROUP BY department;
```

---

## ETL Pipelines

### Extract, Transform, Load

```sql
-- Full ETL pipeline
-- 1. Extract from YAML
CREATE TEMP TABLE raw_data AS
SELECT * FROM (SELECT filename, content::YAML AS data FROM read_text('input/*.yaml'));

-- 2. Transform and validate
CREATE TEMP TABLE transformed AS
SELECT
    filename AS source,
    yaml_extract_string(data, '$.id') AS id,
    yaml_extract_string(data, '$.name') AS name,
    TRY_CAST(yaml_extract_string(data, '$.value') AS DOUBLE) AS value,
    current_timestamp AS loaded_at
FROM raw_data
WHERE yaml_valid(data::VARCHAR)
  AND yaml_exists(data, '$.id');

-- 3. Load to destination
INSERT INTO warehouse_table
SELECT * FROM transformed
WHERE id NOT IN (SELECT id FROM warehouse_table);
```

### Incremental Processing

```sql
-- Track processed files
CREATE TABLE IF NOT EXISTS etl_log (
    filename VARCHAR PRIMARY KEY,
    processed_at TIMESTAMP,
    record_count INTEGER
);

-- Process only new files
WITH new_files AS (
    SELECT filename, data
    FROM (SELECT filename, content::YAML AS data FROM read_text('data/*.yaml'))
    WHERE filename NOT IN (SELECT filename FROM etl_log)
)
INSERT INTO target_table
SELECT yaml_extract_string(data, '$.id') AS id,
       yaml_extract_string(data, '$.value') AS value
FROM new_files;

-- Update log
INSERT INTO etl_log
SELECT filename, current_timestamp, 1
FROM new_files;
```

---

## Backup and Restore

### Backup Table to YAML

```sql
-- Full backup
COPY (
    SELECT * FROM important_table
) TO 'backup/important_table.yaml' (FORMAT yaml, STYLE block);

-- Backup with metadata
COPY (
    SELECT value_to_yaml(struct_pack(
        backup_timestamp := current_timestamp,
        table_name := 'important_table',
        row_count := (SELECT COUNT(*) FROM important_table),
        data := (SELECT list(*) FROM important_table)
    ))
) TO 'backup/important_table_full.yaml' (FORMAT yaml, STYLE block);
```

### Restore from YAML

```sql
-- Restore table
INSERT INTO important_table
SELECT * FROM read_yaml('backup/important_table.yaml');

-- Verify restore
SELECT
    (SELECT COUNT(*) FROM important_table) AS current_count,
    yaml_array_length(
        yaml_extract(data, '$.data')
    ) AS backup_count
FROM (SELECT filename, content::YAML AS data FROM read_text('backup/important_table_full.yaml'));
```
