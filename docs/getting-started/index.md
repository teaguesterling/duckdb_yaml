# Getting Started

Welcome to the DuckDB YAML Extension! This guide will help you get up and running quickly.

## Overview

The YAML extension adds comprehensive YAML support to DuckDB, enabling you to:

- **Read YAML files** directly into tables with automatic schema detection
- **Extract YAML frontmatter** from Markdown and other text files
- **Store YAML data** using a native YAML type
- **Query YAML content** using path-based extraction functions
- **Write query results** to YAML files

## Quick Start

### 1. Install the Extension

```sql
INSTALL yaml FROM community;
LOAD yaml;
```

### 2. Read Your First YAML File

```sql
-- Query a YAML file directly
SELECT * FROM 'config.yaml';

-- Or use the read_yaml function
SELECT * FROM read_yaml('config.yaml');
```

### 3. Work with YAML Data

```sql
-- Create a table with YAML columns
CREATE TABLE settings(name VARCHAR, config YAML);

-- Insert YAML data
INSERT INTO settings VALUES
    ('app', 'port: 8080\ndebug: true'),
    ('db', 'host: localhost\nport: 5432');

-- Query using extraction functions
SELECT
    name,
    yaml_extract(config, '$.port') AS port
FROM settings;
```

## Next Steps

<div class="grid cards" markdown>

-   :material-download:{ .lg .middle } __Installation__

    ---

    Detailed installation instructions for all platforms

    [:octicons-arrow-right-24: Installation Guide](installation.md)

-   :material-rocket-launch:{ .lg .middle } __Quick Start Tutorial__

    ---

    A hands-on tutorial with examples

    [:octicons-arrow-right-24: Quick Start](quickstart.md)

</div>
