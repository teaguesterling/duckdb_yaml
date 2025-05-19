-- Load the YAML extension
LOAD yaml;

-- Check if debug mode is initially disabled
SELECT yaml_debug_is_enabled() AS debug_enabled;

-- Run a simple test with value_to_yaml
SELECT value_to_yaml('Simple string') AS yaml_string;

-- Try a struct with value_to_yaml (likely segfault)
-- SELECT value_to_yaml({'name': 'John', 'age': 30}) AS yaml_struct;

-- Enable debug mode
SELECT yaml_debug_enable() AS debug_mode_enabled;

-- Verify debug mode is enabled
SELECT yaml_debug_is_enabled() AS debug_enabled;

-- Try the same tests with debug mode enabled
SELECT yaml_debug_value_to_yaml('Simple string') AS yaml_string;

-- Try a struct with yaml_debug_value_to_yaml
-- SELECT yaml_debug_value_to_yaml({'name': 'John', 'age': 30}) AS yaml_struct;

-- Disable debug mode
SELECT yaml_debug_disable() AS debug_mode_disabled;

-- Verify debug mode is disabled
SELECT yaml_debug_is_enabled() AS debug_enabled;