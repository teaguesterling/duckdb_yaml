# Frontmatter Extraction

The YAML extension can extract YAML frontmatter from Markdown, MDX, Astro, and other text files.

## What is Frontmatter?

Frontmatter is YAML metadata enclosed between `---` delimiters at the start of a file:

```markdown
---
title: My Blog Post
author: John Doe
date: 2024-01-15
tags: [tutorial, duckdb, yaml]
draft: false
---

# My Blog Post

This is the content of the post...
```

## Basic Usage

```sql
-- Read frontmatter from Markdown files
SELECT * FROM read_yaml_frontmatter('posts/*.md');

-- Returns columns: title, author, date, tags, draft
```

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `as_yaml_objects` | BOOLEAN | `false` | Return raw YAML instead of columns |
| `content` | BOOLEAN | `false` | Include file body after frontmatter |
| `filename` | BOOLEAN | `false` | Include source file path |

### as_yaml_objects

Return raw YAML instead of expanding into columns:

```sql
-- Get raw YAML frontmatter
SELECT frontmatter
FROM read_yaml_frontmatter('posts/*.md', as_yaml_objects := true);
```

### content

Include the file content after the frontmatter:

```sql
SELECT title, content
FROM read_yaml_frontmatter('posts/*.md', content := true);
```

The `content` column contains everything after the closing `---` delimiter.

### filename

Include the source file path:

```sql
SELECT filename, title, date
FROM read_yaml_frontmatter('posts/*.md', filename := true);
```

## Supported File Types

Frontmatter works with any text file using `---` delimiters:

| Format | Extension | Common Use |
|--------|-----------|------------|
| Markdown | `.md` | Blog posts, documentation |
| MDX | `.mdx` | Docusaurus, interactive docs |
| Astro | `.astro` | Astro components |
| reStructuredText | `.rst` | Python documentation |
| Nunjucks | `.njk` | Eleventy templates |
| Plain text | `.txt` | Notes, any text file |

## Schema Merging

When reading multiple files with different frontmatter fields, schemas are automatically merged:

```markdown
<!-- post1.md -->
---
title: First Post
author: Alice
---

<!-- post2.md -->
---
title: Second Post
category: tutorial
---
```

```sql
SELECT * FROM read_yaml_frontmatter('posts/*.md');
-- Returns columns: title, author, category
-- Missing values are NULL
```

## Examples

### Analyzing a Blog

```sql
-- Find all draft posts
SELECT filename, title, date
FROM read_yaml_frontmatter('content/posts/*.md', filename := true)
WHERE draft = true;

-- Get posts by tag
SELECT title, date, tags
FROM read_yaml_frontmatter('content/posts/*.md')
WHERE list_contains(tags, 'tutorial')
ORDER BY date DESC;

-- Count posts by author
SELECT author, COUNT(*) as post_count
FROM read_yaml_frontmatter('content/posts/*.md')
GROUP BY author
ORDER BY post_count DESC;
```

### Documentation Sites

```sql
-- Query Docusaurus documentation structure
SELECT
    filename,
    title,
    sidebar_position,
    tags
FROM read_yaml_frontmatter('docs/**/*.{md,mdx}', filename := true)
ORDER BY sidebar_position;

-- Find pages without titles
SELECT filename
FROM read_yaml_frontmatter('docs/**/*.md', filename := true)
WHERE title IS NULL;
```

### Content Management

```sql
-- Export blog metadata to CSV
COPY (
    SELECT title, author, date, tags
    FROM read_yaml_frontmatter('content/posts/*.md')
    ORDER BY date DESC
) TO 'blog_index.csv';

-- Find outdated posts
SELECT title, date
FROM read_yaml_frontmatter('content/posts/*.md')
WHERE date < '2023-01-01'::DATE;
```

### Full-Text Search Preparation

```sql
-- Create searchable content table
CREATE TABLE searchable_content AS
SELECT
    filename,
    title,
    author,
    date,
    tags,
    content
FROM read_yaml_frontmatter('content/**/*.md',
    filename := true,
    content := true
);

-- Add full-text search index (if using FTS extension)
-- CREATE INDEX ON searchable_content USING fts (title, content);
```

### Static Site Generation Analysis

```sql
-- Astro component analysis
SELECT
    filename,
    yaml_extract_string(frontmatter, '$.layout') as layout,
    yaml_extract_string(frontmatter, '$.title') as title
FROM read_yaml_frontmatter('src/pages/*.astro',
    filename := true,
    as_yaml_objects := true
);

-- Eleventy template analysis
SELECT *
FROM read_yaml_frontmatter('src/**/*.njk');
```

## Working with Raw YAML

When using `as_yaml_objects := true`, you can use extraction functions:

```sql
SELECT
    filename,
    yaml_extract_string(frontmatter, '$.title') as title,
    yaml_extract(frontmatter, '$.tags') as tags,
    yaml_exists(frontmatter, '$.draft') as has_draft_field
FROM read_yaml_frontmatter('posts/*.md',
    filename := true,
    as_yaml_objects := true
);
```

## Error Handling

Files without frontmatter or with invalid YAML are handled gracefully:

```sql
-- Skip files with invalid frontmatter
SELECT *
FROM read_yaml_frontmatter('mixed/*.md', ignore_errors := true);
```

Files without frontmatter (no `---` at start) return NULL for all frontmatter columns.

## See Also

- [Reading YAML Files](reading-yaml.md) - General YAML file reading
- [Extraction Functions](../functions/extraction.md) - Querying YAML values
- [Type Detection](type-detection.md) - How frontmatter values are typed
