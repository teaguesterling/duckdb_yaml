#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

#include "duckdb.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "yaml-cpp/yaml.h"

using namespace duckdb;

// Simplified version of ValueToYAML utilities for testing
namespace yaml_test {

enum class YAMLFormat { 
    BLOCK,  // Multi-line, indented format
    FLOW    // Inline, compact format similar to JSON
};

void ConfigureEmitter(YAML::Emitter& out, YAMLFormat format) {
    out.SetIndent(2);
    if (format == YAMLFormat::FLOW) {
        out.SetMapFormat(YAML::Flow);
        out.SetSeqFormat(YAML::Flow);
    } else {
        out.SetMapFormat(YAML::Block);
        out.SetSeqFormat(YAML::Block);
    }
}

void EmitValueToYAML(YAML::Emitter& out, const Value& value) {
    std::cout << "Processing value of type: " << value.type().ToString() << std::endl;
    
    try {
        if (value.IsNull()) {
            std::cout << "   - Emitting NULL value" << std::endl;
            out << YAML::Null;
            return;
        }
        
        switch (value.type().id()) {
            case LogicalTypeId::VARCHAR: {
                std::string str_val = value.GetValue<string>();
                std::cout << "   - Emitting VARCHAR: '" << str_val << "'" << std::endl;
                
                // Check if the string needs special formatting
                bool needs_quotes = false;
                
                if (str_val.empty()) {
                    needs_quotes = true;
                } else {
                    // Check for special strings that could be interpreted as something else
                    if (str_val == "null" || str_val == "true" || str_val == "false" ||
                        str_val == "yes" || str_val == "no" || str_val == "on" || str_val == "off" ||
                        str_val == "~" || str_val == "") {
                        needs_quotes = true;
                    }
                    
                    // Check if it looks like a number
                    try {
                        size_t pos;
                        std::stod(str_val, &pos);
                        if (pos == str_val.size()) {
                            needs_quotes = true; // It looks like a number
                        }
                    } catch (...) {
                        // Not a number
                    }
                    
                    // Check for special characters
                    for (char c : str_val) {
                        if (c == ':' || c == '{' || c == '}' || c == '[' || c == ']' || 
                            c == ',' || c == '&' || c == '*' || c == '#' || c == '?' || 
                            c == '|' || c == '-' || c == '<' || c == '>' || c == '=' || 
                            c == '!' || c == '%' || c == '@' || c == '\\' || c == '"' ||
                            c == '\'' || c == '\n' || c == '\t' || c == ' ') {
                            needs_quotes = true;
                            break;
                        }
                    }
                }
                
                if (needs_quotes) {
                    // Use single quoted style for most strings requiring quotes
                    out << YAML::SingleQuoted << str_val;
                } else {
                    // Use plain style for strings that don't need quotes
                    out << str_val;
                }
                break;
            }
            case LogicalTypeId::BOOLEAN:
                std::cout << "   - Emitting BOOLEAN: " << (value.GetValue<bool>() ? "true" : "false") << std::endl;
                out << value.GetValue<bool>();
                break;
            case LogicalTypeId::INTEGER:
            case LogicalTypeId::BIGINT:
                try {
                    int64_t int_val = value.GetValue<int64_t>();
                    std::cout << "   - Emitting INTEGER/BIGINT: " << int_val << std::endl;
                    out << int_val;
                } catch (std::exception& e) {
                    std::cout << "   - Exception while emitting INTEGER: " << e.what() << std::endl;
                    out << YAML::SingleQuoted << value.ToString();
                }
                break;
            case LogicalTypeId::FLOAT:
            case LogicalTypeId::DOUBLE:
                try {
                    double dbl_val = value.GetValue<double>();
                    std::cout << "   - Emitting FLOAT/DOUBLE: " << dbl_val << std::endl;
                    out << dbl_val;
                } catch (std::exception& e) {
                    std::cout << "   - Exception while emitting DOUBLE: " << e.what() << std::endl;
                    out << YAML::SingleQuoted << value.ToString();
                }
                break;
            case LogicalTypeId::LIST: {
                std::cout << "   - Emitting LIST" << std::endl;
                try {
                    auto& list_val = ListValue::GetChildren(value);
                    std::cout << "   - List size: " << list_val.size() << std::endl;
                    
                    out << YAML::BeginSeq;
                    for (size_t i = 0; i < list_val.size(); i++) {
                        std::cout << "   - List element " << i << ":" << std::endl;
                        const auto& element = list_val[i];
                        EmitValueToYAML(out, element);
                    }
                    out << YAML::EndSeq;
                } catch (std::exception& e) {
                    std::cout << "   - Exception while emitting LIST: " << e.what() << std::endl;
                    out << YAML::SingleQuoted << value.ToString();
                }
                break;
            }
            case LogicalTypeId::STRUCT: {
                std::cout << "   - Emitting STRUCT" << std::endl;
                try {
                    out << YAML::BeginMap;
                    auto& struct_vals = StructValue::GetChildren(value);
                    auto& struct_names = StructType::GetChildTypes(value.type());
                    
                    std::cout << "   - Struct values size: " << struct_vals.size() << std::endl;
                    std::cout << "   - Struct names size: " << struct_names.size() << std::endl;
                    
                    // Safety check for struct children
                    if (struct_vals.size() != struct_names.size()) {
                        throw std::runtime_error("Mismatch between struct values and names");
                    }
                    
                    for (size_t i = 0; i < struct_vals.size(); i++) {
                        std::cout << "   - Struct field '" << struct_names[i].first << "'" << std::endl;
                        out << YAML::Key << struct_names[i].first;
                        out << YAML::Value;
                        EmitValueToYAML(out, struct_vals[i]);
                    }
                    out << YAML::EndMap;
                } catch (std::exception& e) {
                    std::cout << "   - Exception while emitting STRUCT: " << e.what() << std::endl;
                    out << YAML::SingleQuoted << value.ToString();
                }
                break;
            }
            default:
                std::cout << "   - Emitting default case as string: " << value.ToString() << std::endl;
                out << YAML::SingleQuoted << value.ToString();
                break;
        }
    } catch (std::exception& e) {
        std::cout << "   - Caught exception in EmitValueToYAML: " << e.what() << std::endl;
        try {
            out << YAML::Null;
        } catch (...) {
            // Prevent cascading exceptions
            std::cout << "   - Failed to emit NULL after exception" << std::endl;
        }
    } catch (...) {
        std::cout << "   - Caught unknown exception in EmitValueToYAML" << std::endl;
        try {
            out << YAML::Null;
        } catch (...) {
            // Prevent cascading exceptions
            std::cout << "   - Failed to emit NULL after unknown exception" << std::endl;
        }
    }
}

std::string ValueToYAMLString(const Value& value, YAMLFormat format = YAMLFormat::BLOCK) {
    try {
        std::cout << "Starting ValueToYAMLString for type: " << value.type().ToString() << std::endl;
        
        YAML::Emitter out;
        ConfigureEmitter(out, format);
        EmitValueToYAML(out, value);
        
        std::cout << "Finished emitting, checking state" << std::endl;
        
        // Check if we have a valid YAML string
        if (out.good() && out.c_str() != nullptr) {
            std::string result = out.c_str();
            std::cout << "Emitter good, result length: " << result.length() << std::endl;
            return result;
        } else {
            std::cout << "Emitter not in good state" << std::endl;
            return "null";
        }
    } catch (const std::exception& e) {
        std::cout << "Exception in ValueToYAMLString: " << e.what() << std::endl;
        return "null";
    } catch (...) {
        std::cout << "Unknown exception in ValueToYAMLString" << std::endl;
        return "null";
    }
}

} // namespace yaml_test

// Test function to create various problematic value types
void TestValueToYAML() {
    std::cout << "\n========== Starting ValueToYAML Tests ==========\n" << std::endl;
    
    try {
        // Basic types
        std::cout << "\n--- Test 1: Basic Types ---" << std::endl;
        
        Value null_val = Value();
        std::cout << "Testing NULL value" << std::endl;
        std::string yaml_null = yaml_test::ValueToYAMLString(null_val);
        std::cout << "Result: " << yaml_null << std::endl;
        
        Value int_val = Value::INTEGER(42);
        std::cout << "Testing INTEGER value" << std::endl;
        std::string yaml_int = yaml_test::ValueToYAMLString(int_val);
        std::cout << "Result: " << yaml_int << std::endl;
        
        Value double_val = Value::DOUBLE(3.14159);
        std::cout << "Testing DOUBLE value" << std::endl;
        std::string yaml_double = yaml_test::ValueToYAMLString(double_val);
        std::cout << "Result: " << yaml_double << std::endl;
        
        Value string_val = Value("Simple string");
        std::cout << "Testing VARCHAR value" << std::endl;
        std::string yaml_string = yaml_test::ValueToYAMLString(string_val);
        std::cout << "Result: " << yaml_string << std::endl;
        
        Value bool_val = Value::BOOLEAN(true);
        std::cout << "Testing BOOLEAN value" << std::endl;
        std::string yaml_bool = yaml_test::ValueToYAMLString(bool_val);
        std::cout << "Result: " << yaml_bool << std::endl;
        
        // Special strings
        std::cout << "\n--- Test 2: Special Strings ---" << std::endl;
        
        Value empty_string = Value("");
        std::cout << "Testing empty string" << std::endl;
        std::string yaml_empty = yaml_test::ValueToYAMLString(empty_string);
        std::cout << "Result: " << yaml_empty << std::endl;
        
        Value special_chars = Value("String with: special, characters {and} [brackets]");
        std::cout << "Testing string with special characters" << std::endl;
        std::string yaml_special = yaml_test::ValueToYAMLString(special_chars);
        std::cout << "Result: " << yaml_special << std::endl;
        
        Value multiline_string = Value("Line 1\nLine 2\nLine 3");
        std::cout << "Testing multiline string" << std::endl;
        std::string yaml_multiline = yaml_test::ValueToYAMLString(multiline_string);
        std::cout << "Result: " << yaml_multiline << std::endl;
        
        // Lists
        std::cout << "\n--- Test 3: Lists ---" << std::endl;
        
        // Simple list
        std::vector<Value> simple_list_values = {Value::INTEGER(1), Value::INTEGER(2), Value::INTEGER(3)};
        Value simple_list = Value::LIST(LogicalType::INTEGER, simple_list_values);
        std::cout << "Testing simple list" << std::endl;
        std::string yaml_simple_list = yaml_test::ValueToYAMLString(simple_list);
        std::cout << "Result: " << yaml_simple_list << std::endl;
        
        // Mixed type list
        std::vector<Value> mixed_list_values = {
            Value::INTEGER(1), 
            Value("string"), 
            Value::BOOLEAN(true), 
            Value::DOUBLE(3.14)
        };
        Value mixed_list = Value::LIST(LogicalType::ANY, mixed_list_values);
        std::cout << "Testing mixed type list" << std::endl;
        std::string yaml_mixed_list = yaml_test::ValueToYAMLString(mixed_list);
        std::cout << "Result: " << yaml_mixed_list << std::endl;
        
        // Nested list
        std::vector<Value> inner_list_values = {Value::INTEGER(4), Value::INTEGER(5), Value::INTEGER(6)};
        Value inner_list = Value::LIST(LogicalType::INTEGER, inner_list_values);
        
        std::vector<Value> nested_list_values = {
            Value::INTEGER(1), 
            Value::INTEGER(2), 
            inner_list
        };
        Value nested_list = Value::LIST(LogicalType::ANY, nested_list_values);
        std::cout << "Testing nested list" << std::endl;
        std::string yaml_nested_list = yaml_test::ValueToYAMLString(nested_list);
        std::cout << "Result: " << yaml_nested_list << std::endl;
        
        // Empty list
        std::vector<Value> empty_list_values = {};
        Value empty_list = Value::LIST(LogicalType::INTEGER, empty_list_values);
        std::cout << "Testing empty list" << std::endl;
        std::string yaml_empty_list = yaml_test::ValueToYAMLString(empty_list);
        std::cout << "Result: " << yaml_empty_list << std::endl;
        
        // Structs
        std::cout << "\n--- Test 4: Structs ---" << std::endl;
        
        // Simple struct
        child_list_t<Value> simple_struct_values;
        simple_struct_values.push_back(std::make_pair("id", Value::INTEGER(1)));
        simple_struct_values.push_back(std::make_pair("name", Value("Alice")));
        
        child_list_t<LogicalType> simple_struct_types;
        simple_struct_types.push_back(std::make_pair("id", LogicalType::INTEGER));
        simple_struct_types.push_back(std::make_pair("name", LogicalType::VARCHAR));
        
        Value simple_struct = Value::STRUCT(simple_struct_values, simple_struct_types);
        std::cout << "Testing simple struct" << std::endl;
        std::string yaml_simple_struct = yaml_test::ValueToYAMLString(simple_struct);
        std::cout << "Result: " << yaml_simple_struct << std::endl;
        
        // Nested struct
        child_list_t<Value> inner_struct_values;
        inner_struct_values.push_back(std::make_pair("street", Value("123 Main St")));
        inner_struct_values.push_back(std::make_pair("city", Value("Anytown")));
        
        child_list_t<LogicalType> inner_struct_types;
        inner_struct_types.push_back(std::make_pair("street", LogicalType::VARCHAR));
        inner_struct_types.push_back(std::make_pair("city", LogicalType::VARCHAR));
        
        Value inner_struct = Value::STRUCT(inner_struct_values, inner_struct_types);
        
        child_list_t<Value> nested_struct_values;
        nested_struct_values.push_back(std::make_pair("id", Value::INTEGER(1)));
        nested_struct_values.push_back(std::make_pair("name", Value("Bob")));
        nested_struct_values.push_back(std::make_pair("address", inner_struct));
        
        child_list_t<LogicalType> nested_struct_types;
        nested_struct_types.push_back(std::make_pair("id", LogicalType::INTEGER));
        nested_struct_types.push_back(std::make_pair("name", LogicalType::VARCHAR));
        nested_struct_types.push_back(std::make_pair("address", LogicalType::STRUCT(inner_struct_types)));
        
        Value nested_struct = Value::STRUCT(nested_struct_values, nested_struct_types);
        std::cout << "Testing nested struct" << std::endl;
        std::string yaml_nested_struct = yaml_test::ValueToYAMLString(nested_struct);
        std::cout << "Result: " << yaml_nested_struct << std::endl;
        
        // Complex combinations
        std::cout << "\n--- Test 5: Complex Combinations ---" << std::endl;
        
        // Struct with list
        std::vector<Value> struct_list_values = {Value::INTEGER(1), Value::INTEGER(2), Value::INTEGER(3)};
        Value struct_list = Value::LIST(LogicalType::INTEGER, struct_list_values);
        
        child_list_t<Value> complex_struct_values;
        complex_struct_values.push_back(std::make_pair("id", Value::INTEGER(1)));
        complex_struct_values.push_back(std::make_pair("name", Value("Charlie")));
        complex_struct_values.push_back(std::make_pair("scores", struct_list));
        
        child_list_t<LogicalType> complex_struct_types;
        complex_struct_types.push_back(std::make_pair("id", LogicalType::INTEGER));
        complex_struct_types.push_back(std::make_pair("name", LogicalType::VARCHAR));
        complex_struct_types.push_back(std::make_pair("scores", LogicalType::LIST(LogicalType::INTEGER)));
        
        Value struct_with_list = Value::STRUCT(complex_struct_values, complex_struct_types);
        std::cout << "Testing struct with list" << std::endl;
        std::string yaml_struct_with_list = yaml_test::ValueToYAMLString(struct_with_list);
        std::cout << "Result: " << yaml_struct_with_list << std::endl;
        
        // List of structs
        std::vector<Value> list_of_structs_values = {simple_struct, nested_struct, struct_with_list};
        Value list_of_structs = Value::LIST(LogicalType::ANY, list_of_structs_values);
        std::cout << "Testing list of structs" << std::endl;
        std::string yaml_list_of_structs = yaml_test::ValueToYAMLString(list_of_structs);
        std::cout << "Result: " << yaml_list_of_structs << std::endl;
        
        // Deeply nested combination
        // Create a complex nested structure with multiple levels
        // First, create an inner list for the deepest struct
        std::vector<Value> deep_inner_list_values = {Value::INTEGER(7), Value::INTEGER(8), Value::INTEGER(9)};
        Value deep_inner_list = Value::LIST(LogicalType::INTEGER, deep_inner_list_values);
        
        // Create the deepest struct with a list
        child_list_t<Value> deepest_struct_values;
        deepest_struct_values.push_back(std::make_pair("level", Value("deepest")));
        deepest_struct_values.push_back(std::make_pair("data", deep_inner_list));
        
        child_list_t<LogicalType> deepest_struct_types;
        deepest_struct_types.push_back(std::make_pair("level", LogicalType::VARCHAR));
        deepest_struct_types.push_back(std::make_pair("data", LogicalType::LIST(LogicalType::INTEGER)));
        
        Value deepest_struct = Value::STRUCT(deepest_struct_values, deepest_struct_types);
        
        // Create a mid-level struct containing the deepest struct
        child_list_t<Value> mid_struct_values;
        mid_struct_values.push_back(std::make_pair("level", Value("middle")));
        mid_struct_values.push_back(std::make_pair("nested", deepest_struct));
        
        child_list_t<LogicalType> mid_struct_types;
        mid_struct_types.push_back(std::make_pair("level", LogicalType::VARCHAR));
        mid_struct_types.push_back(std::make_pair("nested", LogicalType::STRUCT(deepest_struct_types)));
        
        Value mid_struct = Value::STRUCT(mid_struct_values, mid_struct_types);
        
        // Create a list containing the mid-level struct
        std::vector<Value> complex_list_values = {mid_struct, mid_struct}; // Add it twice for good measure
        Value complex_list = Value::LIST(LogicalType::ANY, complex_list_values);
        
        // Finally, create the top-level struct
        child_list_t<Value> top_struct_values;
        top_struct_values.push_back(std::make_pair("level", Value("top")));
        top_struct_values.push_back(std::make_pair("complex_data", complex_list));
        
        child_list_t<LogicalType> top_struct_types;
        top_struct_types.push_back(std::make_pair("level", LogicalType::VARCHAR));
        top_struct_types.push_back(std::make_pair("complex_data", LogicalType::LIST(LogicalType::ANY)));
        
        Value deeply_nested = Value::STRUCT(top_struct_values, top_struct_types);
        
        std::cout << "Testing deeply nested structure" << std::endl;
        std::string yaml_deeply_nested = yaml_test::ValueToYAMLString(deeply_nested);
        std::cout << "Result: " << yaml_deeply_nested << std::endl;
        
        // Test with large structures
        std::cout << "\n--- Test 6: Large Structures ---" << std::endl;
        
        // Create a large list
        std::vector<Value> large_list_values;
        for (int i = 0; i < 1000; i++) {
            large_list_values.push_back(Value::INTEGER(i));
        }
        Value large_list = Value::LIST(LogicalType::INTEGER, large_list_values);
        std::cout << "Testing large list (1000 elements)" << std::endl;
        std::string yaml_large_list = yaml_test::ValueToYAMLString(large_list);
        std::cout << "Result length: " << yaml_large_list.length() << " characters" << std::endl;
        
        // Create a deeply recursive structure
        Value recursive = Value::INTEGER(0);
        for (int i = 0; i < 50; i++) {
            child_list_t<Value> rec_values;
            rec_values.push_back(std::make_pair("depth", Value::INTEGER(i)));
            rec_values.push_back(std::make_pair("inner", recursive));
            
            child_list_t<LogicalType> rec_types;
            rec_types.push_back(std::make_pair("depth", LogicalType::INTEGER));
            rec_types.push_back(std::make_pair("inner", recursive.type()));
            
            recursive = Value::STRUCT(rec_values, rec_types);
        }
        
        std::cout << "Testing deeply recursive structure (depth 50)" << std::endl;
        std::string yaml_recursive = yaml_test::ValueToYAMLString(recursive);
        std::cout << "Result length: " << yaml_recursive.length() << " characters" << std::endl;
        
    } catch (std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "UNKNOWN EXCEPTION" << std::endl;
    }
    
    std::cout << "\n========== All Tests Completed ==========\n" << std::endl;
}

int main() {
    std::cout << "Starting ValueToYAML Test Program" << std::endl;
    std::cout << "This program will test the ValueToYAML functionality with different inputs" << std::endl;
    
    try {
        TestValueToYAML();
        std::cout << "All tests completed successfully!" << std::endl;
    } catch (std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "FATAL ERROR: Unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}