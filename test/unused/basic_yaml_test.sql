-- Basic YAML functions test

-- Register YAML extension
LOAD yaml;

-- Validate that the extension is correctly loaded
SELECT 'Extension loaded' AS result;

-- Test yaml_valid function
SELECT yaml_valid('name: John') AS is_valid_yaml;
SELECT yaml_valid('invalid: yaml: structure:') AS is_invalid_yaml;

-- Test multi-document validation
SELECT yaml_valid('---
name: John
---
name: Jane') AS is_valid_multi_doc_yaml;

-- Test YAML to JSON conversion
SELECT yaml_to_json('name: John') AS json_result;
SELECT yaml_to_json('age: 30') AS json_result;
SELECT yaml_to_json('is_valid: true') AS json_result;
SELECT yaml_to_json('data: [1, 2, 3]') AS json_result;
SELECT yaml_to_json('person: {name: John, age: 30}') AS json_result;

-- Test multi-document YAML to JSON conversion
SELECT yaml_to_json('---
name: John
---
name: Jane') AS json_result;

-- Test YAML string loading and casting to JSON
CREATE TABLE yaml_strings(yaml VARCHAR);
INSERT INTO yaml_strings VALUES 
    ('name: John'),
    ('age: 30'),
    ('is_valid: true'),
    ('data: [1, 2, 3]'),
    ('person: {name: John, age: 30}');

-- Cast YAML to JSON
SELECT 
    yaml,
    yaml::JSON AS json_val
FROM yaml_strings;

-- Test JSON to YAML conversion
SELECT json_to_yaml('{"name":"John"}') AS yaml_result;
SELECT json_to_yaml('{"age":30}') AS yaml_result;
SELECT json_to_yaml('{"is_valid":true}') AS yaml_result;
SELECT json_to_yaml('{"data":[1,2,3]}') AS yaml_result;
SELECT json_to_yaml('{"person":{"name":"John","age":30}}') AS yaml_result;

-- Load a YAML file with automatic type detection
CREATE TABLE people AS SELECT * FROM read_yaml('test/sql/sample.yaml', auto_detect=true);

-- Show the table structure
DESCRIBE people;

-- Query the loaded data
SELECT person.name, person.age, person.is_active 
FROM people;

-- Test array access
SELECT person.addresses[0].city AS home_city,
       person.addresses[1].city AS work_city,
       person.tags[0] AS first_tag
FROM people;

-- Test nested object access
SELECT person.skills.programming[0].language AS primary_language,
       person.skills.programming[0].level AS primary_language_level,
       person.preferences.theme AS theme
FROM people;

-- Test loading a YAML file as JSON
CREATE TABLE people_json AS SELECT * FROM yaml_to_json('test/sql/sample.yaml');

-- Show the JSON structure
SELECT * FROM people_json;

-- Query the JSON data using JSON functions
SELECT 
    json_extract_string(json, '$.person.name') AS name,
    json_extract_int(json, '$.person.age') AS age,
    json_extract_boolean(json, '$.person.is_active') AS is_active
FROM people_json;

-- Test multi-document YAML file
CREATE TABLE multi_doc_test AS SELECT * FROM read_yaml('
---
id: 1
name: John
---
id: 2
name: Jane
---
id: 3
name: Bob
', multi_document=true);

SELECT * FROM multi_doc_test;

-- Clean up
DROP TABLE yaml_strings;
DROP TABLE people;
DROP TABLE people_json;
DROP TABLE multi_doc_test;
