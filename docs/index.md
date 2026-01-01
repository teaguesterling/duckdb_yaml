# DuckDB YAML Extension

[![DuckDB Community Extension](https://img.shields.io/badge/yaml-DuckDB_Community_Extension-blue?logo=duckdb)](https://duckdb.org/community_extensions/extensions/yaml.html)

The YAML extension for DuckDB enables seamless reading, writing, and querying of YAML data directly within SQL queries. It provides native YAML type support, comprehensive extraction functions, and automatic type detection.

## Features

<div class="grid cards" markdown>

-   :material-file-document-multiple:{ .lg .middle } __Read YAML Files__

    ---

    Read single files, glob patterns, or lists of YAML files directly into DuckDB tables with automatic schema detection.

    [:octicons-arrow-right-24: Reading YAML](guide/reading-yaml.md)

-   :material-format-text:{ .lg .middle } __Frontmatter Extraction__

    ---

    Extract YAML frontmatter from Markdown, MDX, Astro, and other text files for blog and documentation analysis.

    [:octicons-arrow-right-24: Frontmatter](guide/frontmatter.md)

-   :material-database:{ .lg .middle } __Native YAML Type__

    ---

    Store and manipulate YAML data with a native type that seamlessly converts between YAML, JSON, and VARCHAR.

    [:octicons-arrow-right-24: YAML Type](guide/yaml-type.md)

-   :material-magnify:{ .lg .middle } __Query & Extract__

    ---

    Use path expressions to extract, filter, and transform YAML data with powerful extraction functions.

    [:octicons-arrow-right-24: Extraction Functions](functions/extraction.md)

-   :material-auto-fix:{ .lg .middle } __Smart Type Detection__

    ---

    Automatic detection of dates, times, timestamps, booleans, and optimal numeric types from YAML data.

    [:octicons-arrow-right-24: Type Detection](guide/type-detection.md)

-   :material-file-export:{ .lg .middle } __Write YAML Files__

    ---

    Export query results to YAML files with customizable formatting using `COPY TO` statements.

    [:octicons-arrow-right-24: Writing YAML](guide/writing-yaml.md)

</div>

## Quick Example

```sql
-- Load the extension
LOAD yaml;

-- Query YAML files directly
SELECT * FROM 'data/config.yaml';
SELECT * FROM 'data/*.yml' WHERE active = true;

-- Create a table with YAML column
CREATE TABLE configs(id INTEGER, config YAML);

-- Insert YAML data
INSERT INTO configs VALUES
    (1, 'environment: prod\nport: 8080'),
    (2, '{environment: dev, port: 3000}');

-- Query YAML data using extraction functions
SELECT
    id,
    yaml_extract_string(config, '$.environment') AS env,
    yaml_extract(config, '$.port') AS port
FROM configs;
```

## Installation

=== "Community Extensions"

    ```sql
    INSTALL yaml FROM community;
    LOAD yaml;
    ```

=== "From GitHub Release"

    ```sql
    INSTALL yaml FROM 'https://github.com/teaguesterling/duckdb_yaml/releases/download/v0.1.0/yaml.duckdb_extension';
    LOAD yaml;
    ```

=== "From Source"

    ```bash
    git clone https://github.com/teaguesterling/duckdb_yaml
    cd duckdb_yaml
    make
    ```

## AI-Written Extension

Claude.ai wrote 99% of the code in this project as an experiment. The original working version was written over the course of a weekend and refined periodically until a production-ready state was reached.

## What's Next?

- [Getting Started](getting-started/index.md) - Installation and first steps
- [User Guide](guide/index.md) - Comprehensive usage documentation
- [Function Reference](functions/index.md) - All available functions
- [Examples](examples/index.md) - Real-world usage patterns
