# Blog & Documentation Examples

Examples for working with Markdown frontmatter and documentation systems.

## Blog Analysis

### Basic Frontmatter Query

Given blog posts with frontmatter:

```markdown
---
title: Getting Started with DuckDB
author: Jane Smith
date: 2024-01-15
tags: [duckdb, tutorial, sql]
draft: false
---

# Getting Started with DuckDB

This tutorial covers...
```

Query all posts:

```sql
SELECT title, author, date, tags
FROM read_yaml_frontmatter('content/posts/*.md')
ORDER BY date DESC;
```

### Filter by Properties

```sql
-- Published posts only
SELECT title, date
FROM read_yaml_frontmatter('posts/*.md')
WHERE draft = false OR draft IS NULL
ORDER BY date DESC;

-- Posts by author
SELECT title, date
FROM read_yaml_frontmatter('posts/*.md')
WHERE author = 'Jane Smith';

-- Recent posts
SELECT title, author, date
FROM read_yaml_frontmatter('posts/*.md')
WHERE date >= '2024-01-01'::DATE
ORDER BY date DESC;
```

### Tag Analysis

```sql
-- Find posts by tag
SELECT title, tags
FROM read_yaml_frontmatter('posts/*.md')
WHERE list_contains(tags, 'tutorial');

-- Count posts per tag
SELECT unnest(tags) AS tag, COUNT(*) AS post_count
FROM read_yaml_frontmatter('posts/*.md')
GROUP BY tag
ORDER BY post_count DESC;

-- Find all unique tags
SELECT DISTINCT unnest(tags) AS tag
FROM read_yaml_frontmatter('posts/*.md')
ORDER BY tag;
```

---

## Content Management

### Build a Blog Index

```sql
-- Create searchable index
CREATE TABLE blog_index AS
SELECT
    filename,
    title,
    author,
    date,
    tags,
    description,
    slug
FROM read_yaml_frontmatter('content/posts/*.md', filename = true);

-- Query the index
SELECT title, date FROM blog_index
WHERE description LIKE '%DuckDB%'
ORDER BY date DESC;
```

### Content with Body

```sql
-- Include post content for full-text search
SELECT
    filename,
    title,
    date,
    length(content) AS content_length,
    content
FROM read_yaml_frontmatter('posts/*.md',
    filename = true,
    content = true
);

-- Find posts mentioning specific topics
SELECT title, filename
FROM read_yaml_frontmatter('posts/*.md',
    filename = true,
    content = true
)
WHERE content LIKE '%machine learning%';
```

### Draft Management

```sql
-- List all drafts
SELECT filename, title, author
FROM read_yaml_frontmatter('posts/*.md', filename = true)
WHERE draft = true;

-- Draft statistics
SELECT
    CASE WHEN draft = true THEN 'Draft' ELSE 'Published' END AS status,
    COUNT(*) AS count
FROM read_yaml_frontmatter('posts/*.md')
GROUP BY draft;
```

---

## Documentation Sites

### Docusaurus Analysis

```sql
-- Query Docusaurus docs structure
SELECT
    filename,
    title,
    sidebar_position,
    sidebar_label
FROM read_yaml_frontmatter('docs/**/*.{md,mdx}', filename = true)
ORDER BY sidebar_position NULLS LAST;

-- Find pages missing sidebar position
SELECT filename, title
FROM read_yaml_frontmatter('docs/**/*.md', filename = true)
WHERE sidebar_position IS NULL;
```

### Documentation Validation

```sql
-- Check for missing required fields
SELECT filename,
    title IS NOT NULL AS has_title,
    description IS NOT NULL AS has_description
FROM read_yaml_frontmatter('docs/**/*.md', filename = true)
WHERE title IS NULL OR description IS NULL;

-- Find broken category references
SELECT filename, title, category
FROM read_yaml_frontmatter('docs/**/*.md', filename = true)
WHERE category IS NOT NULL
  AND category NOT IN (SELECT DISTINCT category FROM valid_categories);
```

### Generate Navigation

```sql
-- Build navigation structure
SELECT
    category,
    list(struct_pack(
        title := title,
        path := filename,
        position := sidebar_position
    ) ORDER BY sidebar_position) AS pages
FROM read_yaml_frontmatter('docs/**/*.md', filename = true)
GROUP BY category
ORDER BY category;
```

---

## Static Site Generators

### Astro Components

```sql
-- Analyze Astro pages
SELECT
    filename,
    yaml_extract_string(frontmatter, '$.layout') AS layout,
    yaml_extract_string(frontmatter, '$.title') AS title
FROM read_yaml_frontmatter('src/pages/**/*.astro',
    filename = true,
    as_yaml_objects = true
);
```

### Eleventy (11ty) Templates

```sql
-- Query Nunjucks templates
SELECT *
FROM read_yaml_frontmatter('src/**/*.njk');

-- Check layout usage
SELECT layout, COUNT(*) AS page_count
FROM read_yaml_frontmatter('src/**/*.{njk,md}')
GROUP BY layout
ORDER BY page_count DESC;
```

---

## Export and Reports

### Export Blog Metadata

```sql
-- Export to CSV for external tools
COPY (
    SELECT title, author, date, tags
    FROM read_yaml_frontmatter('posts/*.md')
    ORDER BY date DESC
) TO 'blog_metadata.csv';

-- Export to JSON
COPY (
    SELECT title, author, date, tags
    FROM read_yaml_frontmatter('posts/*.md')
    ORDER BY date DESC
) TO 'blog_metadata.json';
```

### Generate Site Statistics

```sql
SELECT
    'Total Posts' AS metric,
    COUNT(*)::VARCHAR AS value
FROM read_yaml_frontmatter('posts/*.md')
UNION ALL
SELECT
    'Unique Authors',
    COUNT(DISTINCT author)::VARCHAR
FROM read_yaml_frontmatter('posts/*.md')
UNION ALL
SELECT
    'Unique Tags',
    COUNT(DISTINCT unnest(tags))::VARCHAR
FROM read_yaml_frontmatter('posts/*.md')
UNION ALL
SELECT
    'Oldest Post',
    MIN(date)::VARCHAR
FROM read_yaml_frontmatter('posts/*.md')
UNION ALL
SELECT
    'Newest Post',
    MAX(date)::VARCHAR
FROM read_yaml_frontmatter('posts/*.md');
```

### Author Statistics

```sql
SELECT
    author,
    COUNT(*) AS post_count,
    MIN(date) AS first_post,
    MAX(date) AS last_post
FROM read_yaml_frontmatter('posts/*.md')
GROUP BY author
ORDER BY post_count DESC;
```
