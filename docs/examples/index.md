# Examples

Real-world examples and use cases for the YAML extension.

## Use Case Categories

<div class="grid cards" markdown>

-   :material-cog:{ .lg .middle } __Configuration Files__

    ---

    Work with application configs, settings files, and infrastructure definitions.

    [:octicons-arrow-right-24: Configuration Examples](config-files.md)

-   :material-post:{ .lg .middle } __Blog & Documentation__

    ---

    Analyze Markdown frontmatter, manage content, and build documentation indexes.

    [:octicons-arrow-right-24: Blog & Docs Examples](blog-docs.md)

-   :material-database-export:{ .lg .middle } __Data Migration__

    ---

    Import/export data, transform formats, and integrate with other systems.

    [:octicons-arrow-right-24: Migration Examples](data-migration.md)

</div>

## Quick Examples

### Read a Config File

```sql
SELECT
    yaml_extract_string(data, '$.database.host') AS db_host,
    yaml_extract(data, '$.database.port') AS db_port
FROM read_yaml_objects('config.yaml');
```

### Analyze Blog Posts

```sql
SELECT title, author, date, tags
FROM read_yaml_frontmatter('posts/*.md')
WHERE list_contains(tags, 'tutorial')
ORDER BY date DESC;
```

### Export to YAML

```sql
COPY (
    SELECT id, name, settings
    FROM users
    WHERE active = true
) TO 'users.yaml' (FORMAT yaml, STYLE block);
```

### Parse YAML String

```sql
SELECT * FROM parse_yaml('
items:
  - name: Widget
    price: 9.99
  - name: Gadget
    price: 19.99
');
```

### Build YAML Object

```sql
SELECT yaml_build_object(
    'timestamp', current_timestamp,
    'data', value_to_yaml(query_result)
) AS yaml_output
FROM (SELECT * FROM source_table) query_result;
```

## Common Patterns

### Conditional Extraction

```sql
SELECT
    CASE
        WHEN yaml_exists(config, '$.database')
        THEN yaml_extract_string(config, '$.database.host')
        ELSE 'localhost'
    END AS db_host
FROM configs;
```

### Flatten Nested Data

```sql
SELECT
    yaml_extract_string(item, '$.name') AS name,
    yaml_extract(item, '$.price')::DOUBLE AS price
FROM read_yaml_objects('products.yaml'),
     yaml_array_elements(yaml_extract(data, '$.items')) AS item;
```

### Aggregate into YAML

```sql
SELECT category,
       value_to_yaml(list(struct_pack(
           name := product_name,
           price := price
       ))) AS products_yaml
FROM products
GROUP BY category;
```

### Validate Before Processing

```sql
SELECT *
FROM raw_data
WHERE yaml_valid(yaml_column)
  AND yaml_exists(yaml_column::YAML, '$.required_field');
```
