#!/bin/bash

echo "Loading YAML extension..."
echo "LOAD yaml;" > init.sql

echo "Testing basic scalar value handling..."

echo "Test 1: Original implementation with integer (should work)"
./build/release/duckdb -init init.sql -c "SELECT value_to_yaml(42);"

echo "Test 2: Enable debug mode"
./build/release/duckdb -init init.sql -c "SELECT yaml_debug_enable();"

echo "Test 3: Check debug mode status"
./build/release/duckdb -init init.sql -c "SELECT yaml_debug_is_enabled();"

echo "Test 4: Debug implementation with integer"
./build/release/duckdb -init init.sql -c "SELECT yaml_debug_value_to_yaml(42);"

echo "Test 5: Debug implementation with string"
./build/release/duckdb -init init.sql -c "SELECT yaml_debug_value_to_yaml('Simple string');"

echo "Test 6: Debug implementation with boolean"
./build/release/duckdb -init init.sql -c "SELECT yaml_debug_value_to_yaml(TRUE);"

echo "Test 7: Debug implementation with NULL"
./build/release/duckdb -init init.sql -c "SELECT yaml_debug_value_to_yaml(NULL);"

echo "Test 8: Debug implementation with list"
./build/release/duckdb -init init.sql -c "SELECT yaml_debug_value_to_yaml([1, 2, 3]);"

echo "Test 9: Debug implementation with simple struct (may still fail)"
# ./build/release/duckdb -init init.sql -c "SELECT yaml_debug_value_to_yaml({'key': 'value'});"

echo "Test 10: Testing with regular value_to_yaml with debug mode enabled"
./build/release/duckdb -init init.sql -c "SELECT value_to_yaml(42);"

echo "Cleanup..."
rm init.sql

echo "Verification complete!"