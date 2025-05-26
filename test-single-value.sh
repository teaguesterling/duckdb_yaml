#!/bin/bash

echo "Testing single value..."

cat <<EOF > test.sql
LOAD yaml;
SELECT yaml_debug_enable();
SELECT yaml_debug_is_enabled();
SELECT yaml_debug_value_to_yaml(42);
EOF

echo "Running test..."
./build/release/duckdb < test.sql

echo "Testing as single command..."
./build/release/duckdb -c "LOAD yaml; SELECT yaml_debug_enable(); SELECT yaml_debug_value_to_yaml(42);"

echo "Cleanup..."
rm test.sql