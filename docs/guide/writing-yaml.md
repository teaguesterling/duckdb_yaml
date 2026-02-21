# Writing YAML Files

The YAML extension supports exporting query results to YAML files using the `COPY TO` statement.

## Basic Usage

```sql
-- Export table to YAML
COPY (SELECT * FROM my_table) TO 'output.yaml' (FORMAT yaml);
```

## Parameters

| Parameter | Values | Default | Description |
|-----------|--------|---------|-------------|
| `STYLE` | `flow`, `block` | `block` | YAML formatting style |
| `LAYOUT` | `document`, `sequence` | `document` | How rows are organized |
| `MULTILINE` | `auto`, `literal`, `quoted` | `auto` | How multiline strings are emitted |
| `INDENT` | `1`-`10` | `2` | Indentation width for block style |

### STYLE Parameter

Controls the YAML formatting:

=== "Block Style (default)"

    Multi-line format with indentation:

    ```sql
    COPY (SELECT * FROM users) TO 'users.yaml' (FORMAT yaml);
    -- or explicitly:
    COPY (SELECT * FROM users) TO 'users.yaml' (FORMAT yaml, STYLE block);
    ```

    Output:
    ```yaml
    ---
    id: 1
    name: Alice
    email: alice@example.com
    ---
    id: 2
    name: Bob
    email: bob@example.com
    ```

=== "Flow Style"

    Compact, inline format:

    ```sql
    COPY (SELECT * FROM users) TO 'users.yaml' (FORMAT yaml, STYLE flow);
    ```

    Output:
    ```yaml
    {id: 1, name: Alice, email: alice@example.com}
    {id: 2, name: Bob, email: bob@example.com}
    ```

### LAYOUT Parameter

Controls how multiple rows are organized:

=== "Document Layout (default)"

    Each row is a separate YAML document:

    ```sql
    COPY (SELECT * FROM items) TO 'items.yaml' (FORMAT yaml, LAYOUT document);
    ```

    Output (with block style):
    ```yaml
    ---
    id: 1
    name: Item One
    ---
    id: 2
    name: Item Two
    ```

=== "Sequence Layout"

    All rows as items in a YAML sequence:

    ```sql
    COPY (SELECT * FROM items) TO 'items.yaml' (FORMAT yaml, LAYOUT sequence);
    ```

    Output:
    ```yaml
    - id: 1
      name: Item One
    - id: 2
      name: Item Two
    ```

### MULTILINE Parameter

Controls how strings containing newlines are emitted:

=== "Auto (default)"

    Resolves based on the style: `literal` for block style, `quoted` for flow style.

    ```sql
    COPY (SELECT * FROM docs) TO 'output.yaml' (FORMAT yaml);
    ```

=== "Literal"

    Uses YAML literal block scalars (`|`) for multiline strings:

    ```sql
    COPY (SELECT * FROM docs) TO 'output.yaml' (FORMAT yaml, MULTILINE literal);
    ```

    Output:
    ```yaml
    description: |
      This is line one
      This is line two
    ```

=== "Quoted"

    Uses quoted strings with escape sequences:

    ```sql
    COPY (SELECT * FROM docs) TO 'output.yaml' (FORMAT yaml, MULTILINE quoted);
    ```

    Output:
    ```yaml
    description: 'This is line one
      This is line two'
    ```

!!! note
    YAML literal block scalars (`|`) use "clip" chomping, which adds a single trailing newline when the value is read back. This is standard YAML behavior. yaml-cpp does not support `|-` (strip) or `|+` (keep) chomping indicators.

### INDENT Parameter

Controls the indentation width for block style output:

```sql
-- Use 4-space indentation
COPY (SELECT * FROM data) TO 'output.yaml' (FORMAT yaml, STYLE block, INDENT 4);
```

Output:
```yaml
user:
    name: Alice
    address:
        city: New York
```

Valid values are 1 through 10. Default is 2.

## Combining Options

```sql
-- Block style with sequence layout
COPY (SELECT * FROM users)
TO 'users.yaml' (FORMAT yaml, STYLE block, LAYOUT sequence);
```

Output:
```yaml
- id: 1
  name: Alice
  email: alice@example.com
- id: 2
  name: Bob
  email: bob@example.com
```

## Working with Complex Data

### Nested Structures

```sql
-- Create nested structures using struct_pack
COPY (
    SELECT
        id,
        name,
        struct_pack(
            street := address_street,
            city := address_city,
            zip := address_zip
        ) AS address
    FROM customers
) TO 'customers.yaml' (FORMAT yaml, STYLE block);
```

Output:
```yaml
---
id: 1
name: Alice Smith
address:
  street: 123 Main St
  city: New York
  zip: "10001"
```

### Arrays

```sql
COPY (
    SELECT
        category,
        list(product_name) AS products
    FROM products
    GROUP BY category
) TO 'categories.yaml' (FORMAT yaml, STYLE block);
```

Output:
```yaml
---
category: Electronics
products:
  - Laptop
  - Phone
  - Tablet
---
category: Books
products:
  - Novel
  - Textbook
```

## Examples

### Exporting Configuration

```sql
-- Export configuration data
COPY (
    SELECT
        'production' AS environment,
        struct_pack(
            host := 'db.prod.example.com',
            port := 5432,
            database := 'app_prod'
        ) AS database,
        list_value('auth', 'logging', 'metrics') AS features
) TO 'config.yaml' (FORMAT yaml, STYLE block);
```

### Data Migration

```sql
-- Export user data for migration
COPY (
    SELECT
        id,
        username,
        email,
        created_at::VARCHAR AS created_at,
        struct_pack(
            first := first_name,
            last := last_name
        ) AS name,
        preferences
    FROM users
    WHERE active = true
) TO 'users_export.yaml' (FORMAT yaml, STYLE block, LAYOUT sequence);
```

### API Response Templates

```sql
-- Generate API response templates
COPY (
    SELECT
        endpoint,
        method,
        struct_pack(
            status := 200,
            body := response_schema
        ) AS success_response,
        struct_pack(
            status := 400,
            body := error_schema
        ) AS error_response
    FROM api_endpoints
) TO 'api_responses.yaml' (FORMAT yaml, STYLE block);
```

### Report Generation

```sql
-- Generate a summary report
COPY (
    SELECT
        'Sales Report' AS title,
        current_date AS generated_date,
        struct_pack(
            total_sales := SUM(amount),
            order_count := COUNT(*),
            avg_order := AVG(amount)
        ) AS summary,
        list(struct_pack(
            product := product_name,
            quantity := SUM(quantity),
            revenue := SUM(amount)
        )) AS top_products
    FROM orders
    JOIN products USING (product_id)
    GROUP BY ALL
    ORDER BY revenue DESC
    LIMIT 10
) TO 'sales_report.yaml' (FORMAT yaml, STYLE block);
```

## Type Handling

Different DuckDB types are converted to YAML as follows:

| DuckDB Type | YAML Representation |
|-------------|---------------------|
| VARCHAR | String (quoted if needed) |
| INTEGER, BIGINT | Integer |
| DOUBLE, FLOAT | Float |
| BOOLEAN | `true` / `false` |
| DATE | ISO date string |
| TIMESTAMP | ISO timestamp string |
| STRUCT | Mapping |
| LIST | Sequence |
| MAP | Mapping |
| NULL | `null` or omitted |

## Writing to stdout

You can write to stdout for piping or debugging:

```sql
COPY (SELECT * FROM small_table) TO '/dev/stdout' (FORMAT yaml, STYLE block);
```

## Best Practices

### Choose the Right Style

- **Flow style**: Compact, good for simple data or when file size matters
- **Block style**: Readable, good for configuration files or human review

### Choose the Right Layout

- **Document layout**: When each row is independent or for streaming
- **Sequence layout**: When data represents a collection of items

### Handle Special Characters

YAML special characters in strings are automatically quoted:

```sql
-- Strings with colons, quotes, etc. are handled
COPY (SELECT 'value: with special "chars"' AS text)
TO 'special.yaml' (FORMAT yaml);
```

## See Also

- [YAML Type](yaml-type.md) - Working with YAML values
- [Reading YAML Files](reading-yaml.md) - Importing YAML data
- [Conversion Functions](../functions/conversion.md) - Converting to YAML
