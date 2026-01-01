# Construction Functions

Functions for building YAML values from components.

## yaml_build_object

Builds a YAML object from key-value pairs.

### Syntax

```sql
yaml_build_object(key1, value1, key2, value2, ...) → YAML
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `key` | VARCHAR | Object key name |
| `value` | ANY | Value for the key |

Keys and values must be provided in pairs.

### Returns

YAML - A YAML object containing the key-value pairs.

### Examples

#### Basic Usage

```sql
SELECT yaml_build_object('name', 'John', 'age', 30);
-- Returns: {name: John, age: 30}
```

#### Multiple Types

```sql
SELECT yaml_build_object(
    'string', 'hello',
    'number', 42,
    'boolean', true,
    'null_val', NULL
);
-- Returns: {string: hello, number: 42, boolean: true, null_val: null}
```

#### Nested Objects

```sql
SELECT yaml_build_object(
    'user', yaml_build_object('name', 'John', 'email', 'john@example.com'),
    'active', true
);
-- Returns: {user: {name: John, email: john@example.com}, active: true}
```

#### With YAML Values

```sql
SELECT yaml_build_object(
    'config', '{host: localhost, port: 8080}'::YAML,
    'items', '[1, 2, 3]'::YAML
);
-- Returns: {config: {host: localhost, port: 8080}, items: [1, 2, 3]}
```

#### Empty Object

```sql
SELECT yaml_build_object();
-- Returns: {}
```

---

## value_to_yaml

Converts any DuckDB value to YAML format.

!!! note
    This function is also documented in [Conversion Functions](conversion.md), but is included here as it's commonly used for construction.

### Syntax

```sql
value_to_yaml(value) → YAML
```

### Construction Examples

```sql
-- Build from struct
SELECT value_to_yaml({
    'server': {'host': 'localhost', 'port': 8080},
    'features': ['auth', 'logging']
});

-- Build from query result
SELECT value_to_yaml(struct_pack(
    name := 'Product',
    price := 29.99,
    in_stock := true
));

-- Build array
SELECT value_to_yaml(list_value(1, 2, 3, 4, 5));
```

---

## Construction Patterns

### Building from Query Results

```sql
-- Single row to YAML
SELECT value_to_yaml(struct_pack(
    id := id,
    name := name,
    email := email
)) AS yaml_record
FROM users
WHERE id = 1;

-- Multiple rows with aggregation
SELECT yaml_build_object(
    'users', list(value_to_yaml(struct_pack(
        name := name,
        role := role
    )))
) AS yaml_output
FROM users;
```

### Dynamic Key Construction

```sql
-- Build object with dynamic keys
SELECT yaml_build_object(
    category, count(*)
)
FROM products
GROUP BY category;

-- Results in: {Electronics: 50, Books: 30, ...}
```

### Conditional Construction

```sql
SELECT yaml_build_object(
    'name', name,
    'email', email,
    'phone', CASE WHEN phone IS NOT NULL THEN phone END
) AS contact_yaml
FROM contacts;
```

### Merging YAML Objects

```sql
-- Combine multiple objects (via JSON conversion)
SELECT (
    yaml_to_json('{base: config}'::YAML) ||
    yaml_to_json('{override: value}'::YAML)
)::YAML AS merged;
```

### Building Configuration

```sql
-- Create application config
SELECT yaml_build_object(
    'app', yaml_build_object(
        'name', 'MyApp',
        'version', '1.0.0'
    ),
    'database', yaml_build_object(
        'host', db_host,
        'port', db_port,
        'name', db_name
    ),
    'features', list_value('auth', 'logging', 'metrics')
) AS config_yaml
FROM settings;
```

### Template Generation

```sql
-- Generate deployment template
SELECT yaml_build_object(
    'apiVersion', 'apps/v1',
    'kind', 'Deployment',
    'metadata', yaml_build_object(
        'name', service_name,
        'labels', yaml_build_object(
            'app', service_name
        )
    ),
    'spec', yaml_build_object(
        'replicas', replicas,
        'selector', yaml_build_object(
            'matchLabels', yaml_build_object('app', service_name)
        )
    )
) AS k8s_deployment
FROM services;
```

---

## Comparison: yaml_build_object vs value_to_yaml

| Use Case | Recommended Function |
|----------|---------------------|
| Fixed key-value pairs | `yaml_build_object` |
| Converting existing structs | `value_to_yaml` |
| Dynamic keys from columns | `yaml_build_object` |
| Nested complex structures | Either (combine both) |
| Arrays/Lists | `value_to_yaml` |

### Example Comparison

```sql
-- Using yaml_build_object
SELECT yaml_build_object('name', 'John', 'age', 30);

-- Using value_to_yaml (equivalent)
SELECT value_to_yaml({'name': 'John', 'age': 30});

-- Both return: {name: John, age: 30}
```

## See Also

- [Conversion Functions](conversion.md) - Type conversions
- [Writing YAML Files](../guide/writing-yaml.md) - Exporting YAML
- [YAML Type Guide](../guide/yaml-type.md) - Working with YAML type
