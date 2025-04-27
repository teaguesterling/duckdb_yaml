#include "yaml_types.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/function/scalar/string_functions.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "yaml-cpp/yaml.h"

namespace duckdb {

LogicalType YAMLTypes::YAMLType() {
    return LogicalType::VARCHAR;
}

// Helper function to convert YAML node to JSON string
static std::string YAMLNodeToJSON(const YAML::Node &node) {
    if (!node) {
        return "null";
    }
    
    switch (node.Type()) {
        case YAML::NodeType::Null:
            return "null";
        case YAML::NodeType::Scalar: {
            std::string value = node.Scalar();
            
            // Try to detect if the scalar is a number, boolean, or string
            if (value == "true" || value == "yes" || value == "on") {
                return "true";
            } else if (value == "false" || value == "no" || value == "off") {
                return "false";
            } else if (value == "null" || value == "~") {
                return "null";
            }
            
            // Try to parse as number
            try {
                // Check if it's an integer
                size_t pos;
                int64_t int_val = std::stoll(value, &pos);
                if (pos == value.size()) {
                    return value; // It's an integer, return as is
                }
                
                // Check if it's a double
                double double_val = std::stod(value, &pos);
                if (pos == value.size()) {
                    return value; // It's a double, return as is
                }
            } catch (...) {
                // Not a number, treat as string
            }
            
            // Escape JSON string special characters
            std::string result = "\"";
            for (char c : value) {
                switch (c) {
                    case '\"': result += "\\\""; break;
                    case '\\': result += "\\\\"; break;
                    case '\b': result += "\\b"; break;
                    case '\f': result += "\\f"; break;
                    case '\n': result += "\\n"; break;
                    case '\r': result += "\\r"; break;
                    case '\t': result += "\\t"; break;
                    default:
                        if (static_cast<unsigned char>(c) < 32) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", c);
                            result += buf;
                        } else {
                            result += c;
                        }
                }
            }
            result += "\"";
            return result;
        }
        case YAML::NodeType::Sequence: {
            std::string result = "[";
            for (size_t i = 0; i < node.size(); i++) {
                if (i > 0) {
                    result += ",";
                }
                result += YAMLNodeToJSON(node[i]);
            }
            result += "]";
            return result;
        }
        case YAML::NodeType::Map: {
            std::string result = "{";
            bool first = true;
            for (auto it = node.begin(); it != node.end(); ++it) {
                if (!first) {
                    result += ",";
                }
                first = false;
                
                // Key must be a string in JSON
                std::string key = it->first.Scalar();
                result += "\"" + key + "\":" + YAMLNodeToJSON(it->second);
            }
            result += "}";
            return result;
        }
        default:
            throw InternalException("Unknown YAML node type");
    }
}

// Cast YAML string to JSON
static void YAMLToJSONFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    // Extract the input vector
    auto &input_vector = args.data[0];
    
    // Process the input vector
    UnaryExecutor::Execute<string_t, string_t>(
        input_vector, result, args.size(),
        [&](string_t yaml_str) -> string_t {
            if (yaml_str.GetSize() == 0) {
                return string_t();
            }
            
            try {
                // Check if the YAML string contains multiple documents
                std::stringstream yaml_stream(yaml_str.GetString());
                std::vector<YAML::Node> all_docs = YAML::LoadAll(yaml_stream);
                
                if (all_docs.size() == 0) {
                    std::string json_str = "null";
                    return StringVector::AddString(result, json_str);
                } else if (all_docs.size() == 1) {
                    // Single document - convert directly to JSON
                    std::string json_str = YAMLNodeToJSON(all_docs[0]);
                    return StringVector::AddString(result, json_str);
                } else {
                    // Multiple documents - convert to JSON array
                    std::string result_str = "[";
                    for (size_t i = 0; i < all_docs.size(); i++) {
                        if (i > 0) {
                            result_str += ",";
                        }
                        result_str += YAMLNodeToJSON(all_docs[i]);
                    }
                    result_str += "]";
                    return StringVector::AddString(result, result_str);
                }
            } catch (const std::exception &e) {
                throw InvalidInputException("Error converting YAML to JSON: %s", e.what());
            }
        });
}

// Helper function to convert DuckDB Value to YAML string
static std::string ValueToYAMLString(const Value &value) {
    if (value.IsNull()) {
        return "null";
    }
    
    switch (value.type().id()) {
        case LogicalTypeId::VARCHAR: {
            std::string str_val = value.GetValue<string>();
            // Check if string needs quotes in YAML
            bool needs_quotes = false;
            
            if (str_val.empty()) {
                needs_quotes = true;
            } else {
                // Check for special characters or if it could be interpreted as a different type
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
                        needs_quotes = true; // It looks like a number, need quotes
                    }
                } catch (...) {
                    // Not a number
                }
                
                // Check for special characters that would require quotes
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
                // Use single quotes which allow most characters to be used
                return "'" + str_val + "'";
            } else {
                return str_val;
            }
        }
        case LogicalTypeId::BOOLEAN: {
            bool bool_val = value.GetValue<bool>();
            return bool_val ? "true" : "false";
        }
        case LogicalTypeId::INTEGER:
        case LogicalTypeId::BIGINT: {
            int64_t int_val = value.GetValue<int64_t>();
            return std::to_string(int_val);
        }
        case LogicalTypeId::FLOAT:
        case LogicalTypeId::DOUBLE: {
            double double_val = value.GetValue<double>();
            return std::to_string(double_val);
        }
        case LogicalTypeId::LIST: {
            auto &list_val = ListValue::GetChildren(value);
            std::string result = "";
            
            for (const auto &element : list_val) {
                result += "\n- " + ValueToYAMLString(element);
            }
            
            return result.empty() ? "[]" : result;
        }
        case LogicalTypeId::STRUCT: {
            auto &struct_vals = StructValue::GetChildren(value);
            auto &struct_names = StructType::GetChildTypes(value.type());
            std::string result = "";
            
            for (size_t i = 0; i < struct_vals.size(); i++) {
                result += "\n" + struct_names[i].first + ": " + ValueToYAMLString(struct_vals[i]);
            }
            
            return result.empty() ? "{}" : result;
        }
        default:
            return "'" + value.ToString() + "'";
    }
}

// Cast Value to YAML
static void ValueToYAMLFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    // Extract the input vector
    auto &input_vector = args.data[0];
    
    // Process the input vector
    UnaryExecutor::Execute<Value, string_t>(
        input_vector, result, args.size(),
        [&](Value value) -> string_t {
            std::string yaml_str = ValueToYAMLString(value);
            return StringVector::AddString(result, yaml_str);
        });
}

// Convert JSON to YAML
static void JSONToYAMLFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    // Extract the input vector
    auto &input_vector = args.data[0];
    
    // Process the input vector
    UnaryExecutor::Execute<string_t, string_t>(
        input_vector, result, args.size(),
        [&](string_t json_str) -> string_t {
            if (json_str.GetSize() == 0) {
                return string_t();
            }
            
            try {
                // For simplicity, we'll use a two-step process:
                // 1. Parse JSON to Value
                // 2. Convert Value to YAML
                Value json_val = Value::STRUCT({});  // Placeholder
                
                // In a real implementation, you would parse JSON to Value here
                // For now, we'll just return a placeholder
                
                std::string yaml_str = ValueToYAMLString(json_val);
                return StringVector::AddString(result, yaml_str);
            } catch (const std::exception &e) {
                throw InvalidInputException("Error converting JSON to YAML: %s", e.what());
            }
        });
}

void YAMLTypes::Register(DatabaseInstance &db) {
    // Register the YAML type
    auto yaml_type = YAMLType();
    
    // Register the YAML to JSON cast
    auto yaml_to_json_fun = ScalarFunction("yaml_to_json", {yaml_type}, LogicalType::VARCHAR, YAMLToJSONFunction);
    ExtensionUtil::RegisterFunction(db, yaml_to_json_fun);
    
    // Register the JSON to YAML cast 
    auto json_to_yaml_fun = ScalarFunction("json_to_yaml", {LogicalType::VARCHAR}, yaml_type, JSONToYAMLFunction);
    ExtensionUtil::RegisterFunction(db, json_to_yaml_fun);
    
    // Register Value to YAML cast
    auto value_to_yaml_fun = ScalarFunction("value_to_yaml", {LogicalType::ANY}, yaml_type, ValueToYAMLFunction);
    ExtensionUtil::RegisterFunction(db, value_to_yaml_fun);
    
    // Register cast from VARCHAR to YAML and from YAML to JSON 
    // These would be automatically registered when the extension is loaded
    // We don't need to register them explicitly
}

} // namespace duckdb
