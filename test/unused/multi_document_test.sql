-- Test for multi-document YAML file processing

-- Register YAML extension
LOAD yaml;

-- Test reading multi-document YAML file
CREATE TABLE complex_docs AS 
    SELECT * FROM read_yaml('test/sql/complex_sample.yaml', multi_document=true);

-- Overview of documents
SELECT COUNT(*) AS document_count FROM complex_docs;

-- Analyze document types
SELECT 
    type,
    COUNT(*) AS count
FROM complex_docs
GROUP BY type
ORDER BY count DESC;

-- Convert to JSON for detailed querying
CREATE TABLE complex_json AS
    SELECT yaml_to_json(yaml) AS json FROM read_yaml('test/sql/complex_sample.yaml', multi_document=true, auto_detect=false);

-- Extract specific values using JSON functions
SELECT
    json_extract_string(json, '$.type') AS doc_type,
    json_extract_string(json, '$.metadata.version') AS version,
    json_extract_string(json, '$.metadata.created_at') AS created_at
FROM complex_json;

-- Extract user information from the first document
WITH user_doc AS (
    SELECT json FROM complex_json
    WHERE json_extract_string(json, '$.type') = 'user'
    LIMIT 1
)
SELECT
    json_extract_string(json, '$.user.username') AS username,
    json_extract_string(json, '$.user.email') AS email,
    json_extract_string(json, '$.user.profile.first_name') AS first_name,
    json_extract_string(json, '$.user.profile.last_name') AS last_name,
    json_extract_int(json, '$.user.profile.age') AS age
FROM user_doc;

-- Extract project information from the second document
WITH project_doc AS (
    SELECT json FROM complex_json
    WHERE json_extract_string(json, '$.type') = 'project'
    LIMIT 1
)
SELECT
    json_extract_string(json, '$.project.name') AS project_name,
    json_extract_string(json, '$.project.status') AS status,
    json_extract_string(json, '$.project.priority') AS priority,
    json_extract_string(json, '$.project.start_date') AS start_date,
    json_extract_string(json, '$.project.end_date') AS end_date,
    json_extract_float(json, '$.project.budget') AS budget
FROM project_doc;

-- Extract config information from the third document
WITH config_doc AS (
    SELECT json FROM complex_json
    WHERE json_extract_string(json, '$.type') = 'config'
    LIMIT 1
)
SELECT
    json_extract_string(json, '$.config.environment') AS environment,
    json_extract_boolean(json, '$.config.debug') AS debug,
    json_extract_boolean(json, '$.config.features.advanced_analytics') AS advanced_analytics,
    json_extract_boolean(json, '$.config.features.real_time_updates') AS real_time_updates,
    json_extract_string(json, '$.config.database.host') AS db_host,
    json_extract_int(json, '$.config.database.port') AS db_port
FROM config_doc;

-- Working with complex arrays
WITH project_doc AS (
    SELECT json FROM complex_json
    WHERE json_extract_string(json, '$.type') = 'project'
    LIMIT 1
)
SELECT
    json_extract_string(json, '$.project.team[' || (i-1)::VARCHAR || '].role') AS role,
    json_extract_float(json, '$.project.team[' || (i-1)::VARCHAR || '].allocation') AS allocation
FROM project_doc, GENERATE_SERIES(1, json_array_length(json_extract(json, '$.project.team'))) i;

-- Test loading the complex YAML with auto-detection
CREATE TABLE auto_detect_test AS 
    SELECT * FROM read_yaml('test/sql/complex_sample.yaml', multi_document=true, auto_detect=true);

-- Show structure of auto-detected table
DESCRIBE auto_detect_test;

-- Verify data is correctly loaded with auto-detection
SELECT type, metadata.version FROM auto_detect_test;

-- Clean up
DROP TABLE complex_docs;
DROP TABLE complex_json;
DROP TABLE auto_detect_test;
