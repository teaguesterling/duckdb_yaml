#!/bin/bash

echo "Loading YAML extension..."
echo "LOAD yaml;" > init.sql

# Run in normal mode first (try/catch to keep going)
echo "=== RUNNING NORMAL MODE TESTS ==="
echo "Running queries in normal mode..."

while read -r LINE; do
  echo -n "Running: $LINE ... "
  
  # Try to run the query with error handling
  OUTPUT=$(build/release/duckdb -init init.sql -c "$LINE" 2>&1)
  EXIT_CODE=$?
  
  if [ $EXIT_CODE -eq 0 ]; then
    echo "SUCCESS"
  else
    echo "FAILED with exit code $EXIT_CODE"
    echo "Error output: $OUTPUT"
    
    # If it looks like a segfault, print a clearer message
    if [[ $OUTPUT == *"Segmentation fault"* ]]; then
      echo "!!! SEGMENTATION FAULT DETECTED !!!"
      echo "The failing query was: $LINE"
      echo "---------------------------------"
      
      # Enable debug mode and try the same query
      echo "Enabling debug mode and retrying:"
      echo "SELECT yaml_debug_enable();" > debug_mode.sql
      build/release/duckdb -init init.sql -file debug_mode.sql
      
      echo "Retrying with debug mode: $LINE"
      RETRY_OUTPUT=$(build/release/duckdb -init init.sql -c "$LINE" 2>&1)
      RETRY_EXIT_CODE=$?
      
      if [ $RETRY_EXIT_CODE -eq 0 ]; then
        echo "SUCCESS with debug mode enabled"
      else
        echo "FAILED with debug mode too - exit code $RETRY_EXIT_CODE"
        echo "Error output: $RETRY_OUTPUT"
      fi
      
      # Optional - break here or continue
      echo "Do you want to continue testing? (y/n)"
      read answer
      if [[ "$answer" != "y" ]]; then
        echo "Stopping test sequence"
        exit 1
      fi
    fi
  fi
done < test-sequence

echo "Test sequence completed."
rm init.sql
rm -f debug_mode.sql