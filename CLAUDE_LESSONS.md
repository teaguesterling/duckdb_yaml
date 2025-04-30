# CLAUDE_LESSONS.md - Technical Implementation Notes and Lessons

This document contains detailed technical notes and lessons learned while developing the DuckDB YAML extension. It serves as a reference for specific implementation challenges, solutions, and insights that might be useful for future development.

## Direct File Path Support - API Compatibility Issue (May 2025)

### Problem Description

When implementing direct file path support to allow syntax like `SELECT * FROM 'file.yaml'`, we initially attempted to use an approach modeled after examples from other extensions. This approach involved:

1. Using `DBConfig::RegisterFileExtension` to register handlers for `.yaml` and `.yml` extensions
2. Creating a complex lambda function that would build AST nodes using `TableFunctionRef` and `ConstantExpression`
3. Setting properties like `function_name` and `expression_parameters` on the `TableFunctionRef` object

The CI process reported these errors:

```
Check failure on line 51 in src/yaml_extension.cpp
no member named 'function_name' in 'duckdb::TableFunctionRef'

Check failure on line 52 in src/yaml_extension.cpp
no member named 'expression_parameters' in 'duckdb::TableFunctionRef'

Check failure on line 58 in src/yaml_extension.cpp
no member named 'RegisterFileExtension' in 'duckdb::DBConfig'

Check failure on line 59 in src/yaml_extension.cpp
no member named 'RegisterFileExtension' in 'duckdb::DBConfig'
```

These errors indicated that our implementation was incompatible with the current DuckDB API, likely because the API had evolved since the examples we were referencing were written.

### Solution

We revised the implementation to use a much simpler approach:

```cpp
// Register .yaml and .yml extensions to use read_yaml function
fs.RegisterSubstrait("yaml", "read_yaml");
fs.RegisterSubstrait("yml", "read_yaml");
```

This single method call accomplishes what we were trying to do with the more complex approach. It tells DuckDB that files with `.yaml` or `.yml` extensions should be processed using the `read_yaml` function.

### Key Insights

1. **API Evolution**: DuckDB's API is actively evolving. Methods, classes, and properties can change between versions. Always check the current API documentation or source code rather than relying on potentially outdated examples.

2. **Simplicity Wins**: The simpler approach not only works better with the current API but is also more maintainable and less prone to breaking with future API changes.

3. **RegisterSubstrait Method**: This method specifically exists to map file extensions to function names, making our complex AST manipulation unnecessary.

4. **Code Review and CI Process**: The CI process was crucial in catching these issues early. Always pay attention to CI errors and don't assume they're due to infrastructure issues.

5. **Abstraction Boundaries**: This experience highlighted the importance of understanding where the abstraction boundaries lie in DuckDB's extension API. Direct file path support is handled at a higher level than we initially assumed.

### Testing Considerations

Our revised tests focus on core functionality without relying on more complex operations. This ensures we're testing the feature itself rather than specific implementation details that might change.

Key test areas:
- Basic file loading
- Pattern matching (globs)
- Filtering and aggregation
- Table creation

### Future Considerations

If DuckDB's file extension handling API changes in the future, we may need to revisit this implementation. Areas to monitor:

1. Parameter passing through direct file paths (e.g., `FROM 'file.yaml' (param=value)`)
2. Changes to how file extensions are registered or handled
3. Enhanced capabilities for direct file references

### References

- DuckDB's FileSystem class documentation
- Other extensions using RegisterSubstrait (e.g., JSON, CSV handlers)
- DuckDB's parser implementation for direct file references

## Multi-Document YAML Handling and Error Recovery (April 2025)

### Problem Description

YAML files can contain multiple documents separated by `---` markers. Additionally, a YAML file might have syntax errors in one document but valid content in others. We needed to implement robust handling for both of these cases.

Initially, we attempted to use yaml-cpp's `YAML::LoadAll` function to handle multi-document files, but this would fail entirely if any document had syntax errors. This wasn't ideal for our use case, where users might want to extract valid data even from partially corrupted files.

### Solution

We implemented a custom document recovery mechanism:

```cpp
vector<YAML::Node> YAMLReader::RecoverPartialYAMLDocuments(const string &yaml_content) {
    vector<YAML::Node> valid_docs;

    // Normalize newlines
    string normalized_content = yaml_content;
    StringUtil::Replace(normalized_content, "\r\n", "\n");

    // Find document separators and split content
    vector<string> doc_strings;
    size_t pos = 0;
    size_t prev_pos = 0;
    const string doc_separator = "\n---";
    
    // Special handling for document start
    if (normalized_content.compare(0, 3, "---") == 0) {
        prev_pos = 0;
    } else {
        // Extract first document
        pos = normalized_content.find(doc_separator);
        if (pos != string::npos) {
            doc_strings.push_back(normalized_content.substr(0, pos));
            prev_pos = pos + 1;
        } else {
            doc_strings.push_back(normalized_content);
        }
    }

    // Find remaining document separators
    while ((pos = normalized_content.find(doc_separator, prev_pos)) != string::npos) {
        doc_strings.push_back(normalized_content.substr(prev_pos, pos - prev_pos));
        prev_pos = pos + doc_separator.length();
    }

    // Add the last document
    if (prev_pos < normalized_content.length()) {
        doc_strings.push_back(normalized_content.substr(prev_pos));
    }

    // Try to parse each document
    for (const auto &doc_str : doc_strings) {
        try {
            // Skip empty documents or just whitespace/comments
            string trimmed = doc_str;
            StringUtil::Trim(trimmed);
            if (trimmed.empty() || trimmed[0] == '#') {
                continue;
            }

            // Add separator back for proper YAML parsing
            string to_parse = doc_str;
            if (to_parse.compare(0, 3, "---") != 0 && &doc_str != &doc_strings[0]) {
                to_parse = "---" + to_parse;
            }

            YAML::Node doc = YAML::Load(to_parse);
            if (doc.IsDefined() && !doc.IsNull()) {
                valid_docs.push_back(doc);
            }
        } catch (const YAML::Exception &) {
            // Skip invalid documents
            continue;
        }
    }

    return valid_docs;
}
```

This implementation:
1. Handles document splitting manually by looking for separator markers
2. Tries to parse each document individually
3. Skips documents that have syntax errors
4. Returns only the valid documents

### Key Insights

1. **Robust Error Handling**: In data processing extensions, error recovery is often as important as correct parsing. Users may have large or important datasets with minor corruption.

2. **Document Independence**: YAML's multi-document format creates natural boundaries for error recovery. Since each document is independent, we can parse them separately.

3. **Manual String Processing**: Sometimes low-level string handling is necessary when dealing with text-based formats. The document separator is a textual marker, so we need string search operations.

4. **Normalization**: Different systems use different line endings. Normalizing to `\n` simplifies the search for document separators.

5. **Empty Document Handling**: Watch out for edge cases like empty documents, whitespace-only documents, or documents containing only comments.

### Testing Considerations

Testing error recovery requires careful preparation of test cases:

1. Files with mixed valid and invalid documents
2. Files with edge cases (empty documents, only whitespace, etc.)
3. Files with unusual document separator patterns
4. Large files with many documents
5. Files with extreme corruption

### Future Considerations

The current implementation is quite robust but could be improved in several ways:

1. More detailed error messages that indicate which documents failed and why
2. Line/column information for syntax errors
3. Warning system for recovered documents that might have lost information
4. Performance optimization for very large multi-document files

### References

- YAML specification on document separators
- yaml-cpp documentation on error handling
- DuckDB's approach to error handling in other file formats

## Type Detection and Conversion in YAML (April 2025)

### Problem Description

YAML has complex type detection rules. Unlike JSON, which has clear markers for strings, numbers, and booleans, YAML relies more on inference from unquoted values. For example, `true`, `yes`, and `on` are all interpreted as boolean true values.

We needed to implement type detection that would:
1. Work with DuckDB's type system
2. Handle YAML's loose type semantics
3. Support complex nested structures like maps and sequences
4. Allow for type coercion where appropriate

### Solution

We implemented a recursive type detection system:

```cpp
LogicalType YAMLReader::DetectYAMLType(const YAML::Node &node) {
    if (!node) {
        return LogicalType::VARCHAR;
    }

    switch (node.Type()) {
        case YAML::NodeType::Scalar: {
            std::string scalar_value = node.Scalar();

            // Boolean detection
            if (scalar_value == "true" || scalar_value == "false" || 
                scalar_value == "yes" || scalar_value == "no") {
                return LogicalType::BOOLEAN;
            }

            // Number detection
            try {
                // Try integer
                size_t pos;
                std::stoll(scalar_value, &pos);
                if (pos == scalar_value.size()) {
                    return LogicalType::BIGINT;
                }

                // Try double
                std::stod(scalar_value, &pos);
                if (pos == scalar_value.size()) {
                    return LogicalType::DOUBLE;
                }
            } catch (...) {
                // Not a number
            }

            return LogicalType::VARCHAR;
        }
        case YAML::NodeType::Sequence: {
            if (node.size() == 0) {
                return LogicalType::LIST(LogicalType::VARCHAR);
            }

            // Use first element for type detection
            YAML::Node first_element = node[0];
            LogicalType element_type = DetectYAMLType(first_element);
            return LogicalType::LIST(element_type);
        }
        case YAML::NodeType::Map: {
            child_list_t<LogicalType> struct_children;
            for (auto it = node.begin(); it != node.end(); ++it) {
                std::string key = it->first.Scalar();
                LogicalType value_type = DetectYAMLType(it->second);
                struct_children.push_back(make_pair(key, value_type));
            }
            return LogicalType::STRUCT(struct_children);
        }
        default:
            return LogicalType::VARCHAR;
    }
}
```

And a corresponding value conversion function:

```cpp
Value YAMLReader::YAMLNodeToValue(const YAML::Node &node, const LogicalType &target_type) {
    if (!node) {
        return Value(target_type); // NULL value
    }

    switch (node.Type()) {
        case YAML::NodeType::Scalar: {
            std::string scalar_value = node.Scalar();

            if (target_type.id() == LogicalTypeId::VARCHAR) {
                return Value(scalar_value);
            } else if (target_type.id() == LogicalTypeId::BOOLEAN) {
                if (scalar_value == "true" || scalar_value == "yes" || scalar_value == "on") {
                    return Value::BOOLEAN(true);
                } else if (scalar_value == "false" || scalar_value == "no" || scalar_value == "off") {
                    return Value::BOOLEAN(false);
                }
                return Value(target_type); // NULL if not valid boolean
            } else if (target_type.id() == LogicalTypeId::BIGINT) {
                try {
                    return Value::BIGINT(std::stoll(scalar_value));
                } catch (...) {
                    return Value(target_type); // NULL if conversion fails
                }
            } else if (target_type.id() == LogicalTypeId::DOUBLE) {
                try {
                    return Value::DOUBLE(std::stod(scalar_value));
                } catch (...) {
                    return Value(target_type); // NULL if conversion fails
                }
            }
            return Value(scalar_value); // Default to string
        }
        case YAML::NodeType::Sequence: {
            if (target_type.id() != LogicalTypeId::LIST) {
                return Value(target_type); // NULL if not expecting a list
            }

            auto child_type = ListType::GetChildType(target_type);
            vector<Value> values;
            for (size_t i = 0; i < node.size(); i++) {
                values.push_back(YAMLNodeToValue(node[i], child_type));
            }

            return Value::LIST(values);
        }
        case YAML::NodeType::Map: {
            if (target_type.id() != LogicalTypeId::STRUCT) {
                return Value(target_type); // NULL if not expecting a struct
            }

            auto &struct_children = StructType::GetChildTypes(target_type);
            child_list_t<Value> struct_values;
            for (auto &entry : struct_children) {
                if (node[entry.first]) {
                    struct_values.push_back(make_pair(
                        entry.first, 
                        YAMLNodeToValue(node[entry.first], entry.second)
                    ));
                } else {
                    struct_values.push_back(make_pair(
                        entry.first, 
                        Value(entry.second) // NULL value
                    ));
                }
            }

            return Value::STRUCT(struct_values);
        }
        default:
            return Value(target_type); // NULL for unknown type
    }
}
```

### Key Insights

1. **Recursive Type Detection**: For complex nested structures, recursive detection is necessary to preserve the hierarchy.

2. **Type Inference Challenges**: YAML's flexible type system means more ambiguity. We need to try multiple conversions and handle failures gracefully.

3. **Default to Strings**: When in doubt, defaulting to string (VARCHAR) type is the safest approach.

4. **DuckDB Type System**: Understanding DuckDB's type system is crucial - particularly the way LIST and STRUCT types are constructed and accessed.

5. **Type Stability**: We need to maintain consistent types across different documents, defaulting to more flexible types when there are conflicts.

### Testing Considerations

Testing type detection requires:

1. Testing each scalar type (string, integer, double, boolean)
2. Testing type conversions and edge cases
3. Testing nested structures (lists and maps)
4. Testing type conflicts (e.g., same field with different types in different documents)
5. Testing with the auto_detect parameter on and off

### Future Considerations

The current implementation could be enhanced:

1. Support for more specific types (DATE, TIMESTAMP, etc.)
2. Type inference based on all values, not just the first one
3. Smarter handling of type conflicts
4. User-specified type mapping for specific fields
5. Special handling for common YAML patterns (e.g., ISO dates)

### References

- YAML specification on type handling
- yaml-cpp documentation on Node types
- DuckDB's type system documentation
