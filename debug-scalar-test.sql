LOAD yaml;

-- Enable debug mode
SELECT yaml_debug_enable();

-- Test various scalar types to see what's happening
SELECT 'Testing integer 42' as test;
SELECT yaml_debug_value_to_yaml(42);

SELECT 'Testing integer 0' as test;
SELECT yaml_debug_value_to_yaml(0);

SELECT 'Testing boolean true' as test;
SELECT yaml_debug_value_to_yaml(true);

SELECT 'Testing boolean false' as test;
SELECT yaml_debug_value_to_yaml(false);

SELECT 'Testing float 3.14' as test;
SELECT yaml_debug_value_to_yaml(3.14);

SELECT 'Testing string "hello"' as test;
SELECT yaml_debug_value_to_yaml('hello');

SELECT 'Testing null' as test;
SELECT yaml_debug_value_to_yaml(NULL);

-- Let's check what type DuckDB thinks these are
SELECT 'Type of 42:' as test;
SELECT typeof(42);

SELECT 'Type of 3.14:' as test;
SELECT typeof(3.14);

SELECT 'Type of true:' as test;
SELECT typeof(true);

-- Compare with original implementation
SELECT 'Original implementation with 42:' as test;
SELECT value_to_yaml(42);