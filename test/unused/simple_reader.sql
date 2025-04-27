-- test/sql/basic_read_test.sql
-- Basic YAML reading test

-- Register YAML extension
LOAD yaml;

-- Validate that the extension is correctly loaded
SELECT 'Extension loaded' AS result;

-- Test reading a simple YAML file
CREATE TABLE simple_yaml AS 
SELECT * FROM read_yaml('
name: John
age: 30
is_active: true
tags:
  - developer
  - mentor
');

-- Show the structure
DESCRIBE simple_yaml;

-- Query the data
SELECT * FROM simple_yaml;

-- Test with multi-document YAML
CREATE TABLE multi_doc AS 
SELECT * FROM read_yaml('
---
id: 1
name: John
---
id: 2
name: Jane
', multi_document=true);

-- Show the multi-document structure
DESCRIBE multi_doc;

-- Query the multi-document data
SELECT * FROM multi_doc;
