#include "yaml_types.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/function/scalar/string_functions.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "yaml-cpp/yaml.h"

namespace duckdb {

LogicalType YAMLTypes::YAMLType() {
    auto yaml_type = LogicalType(LogicalTypeId::VARCHAR);
    yaml_type.SetAlias("yaml");
    return yaml_type;
}

static bool IsYAMLType(const LogicalType &t) {
    return t.id() == LogicalTypeId::VARCHAR && t.HasAlias() && t.GetAlias() == "yaml";
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

// Forward declaration for recursive emitter function
static void EmitValueToYAML(YAML::Emitter &out, const Value &value);

// Helper function to convert DuckDB Value to YAML string using YAML::Emitter
static std::string ValueToYAMLString(const Value &value) {
    YAML::Emitter out;
    
    // Configure emitter options
    out.SetIndent(2);
    out.SetMapFormat(YAML::Block);
    out.SetSeqFormat(YAML::Block);
    
    // Emit the value
    EmitValueToYAML(out, value);
    
    // Return the resulting YAML string
    return out.c_str();
}

// Recursive function to emit a Value to YAML
static void EmitValueToYAML(YAML::Emitter &out, const Value &value) {
    if (value.IsNull()) {
        out << YAML::Null;
        return;
    }
    
    switch (value.type().id()) {
        case LogicalTypeId::VARCHAR: {
            std::string str_val = value.GetValue<string>();
            
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
            out << value.GetValue<bool>();
            break;
        case LogicalTypeId::INTEGER:
        case LogicalTypeId::BIGINT:
            out << value.GetValue<int64_t>();
            break;
        case LogicalTypeId::FLOAT:
        case LogicalTypeId::DOUBLE:
            out << value.GetValue<double>();
            break;
        case LogicalTypeId::LIST: {
            out << YAML::BeginSeq;
            auto &list_val = ListValue::GetChildren(value);
            for (const auto &element : list_val) {
                EmitValueToYAML(out, element);
            }
            out << YAML::EndSeq;
            break;
        }
        case LogicalTypeId::STRUCT: {
            out << YAML::BeginMap;
            auto &struct_vals = StructValue::GetChildren(value);
            auto &struct_names = StructType::GetChildTypes(value.type());
            for (size_t i = 0; i < struct_vals.size(); i++) {
                out << YAML::Key << struct_names[i].first;
                out << YAML::Value;
                EmitValueToYAML(out, struct_vals[i]);
            }
            out << YAML::EndMap;
            break;
        }
        default:
            // For types we don't handle specifically, convert to string
            out << YAML::SingleQuoted << value.ToString();
            break;
    }
}

// Cast YAML string to JSON - Vector-based function implementation
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
                    return StringVector::AddString(result, json_str.c_str(), json_str.length());
                } else if (all_docs.size() == 1) {
                    // Single document - convert directly to JSON
                    std::string json_str = YAMLNodeToJSON(all_docs[0]);
                    return StringVector::AddString(result, json_str.c_str(), json_str.length());
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
                    return StringVector::AddString(result, result_str.c_str(), result_str.length());
                }
            } catch (const std::exception &e) {
                throw InvalidInputException("Error converting YAML to JSON: %s", e.what());
            }
        });
}

// Convert Value to YAML - Vector-based function implementation
static void ValueToYAMLFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    // Extract the input vector
    auto &input_vector = args.data[0];
    
    // Process the input vector
    UnaryExecutor::Execute<Value, string_t>(
        input_vector, result, args.size(),
        [&](Value value) -> string_t {
            std::string yaml_str = ValueToYAMLString(value);
            return StringVector::AddString(result, yaml_str.c_str(), yaml_str.length());
        });
}

// Convert JSON to YAML - Vector-based function implementation
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
                // Convert JSON to Value
                Value json_val;
                
                // Try to parse JSON - in this version we'll just use the string directly
                // since there's no simple JSON to Value conversion available
                try {
                    std::string json_string = json_str.GetString();
                    json_val = Value(json_string);
                } catch (const std::exception &e) {
                    throw InvalidInputException("Error parsing JSON: %s", e.what());
                }
                
                // Convert Value to YAML using YAML::Emitter
                std::string yaml_str = ValueToYAMLString(json_val);
                return StringVector::AddString(result, yaml_str.c_str(), yaml_str.length());
            } catch (const std::exception &e) {
                throw InvalidInputException("Error converting JSON to YAML: %s", e.what());
            }
        });
}


// Cast from YAML to JSON
static bool YAMLToJSONCast(Vector &source, Vector &result, idx_t count, CastParameters &parameters) {
    UnaryExecutor::Execute<string_t, string_t>(
        source, result, count, [&](string_t yaml_str) -> string_t {
            if (yaml_str.GetSize() == 0) {
                return string_t();
            }
            
            try {
                // Process as multi-document YAML
                std::stringstream yaml_stream(yaml_str.GetString());
                std::vector<YAML::Node> all_docs = YAML::LoadAll(yaml_stream);
                
                std::string json_str;
                if (all_docs.size() == 0) {
                    json_str = "null";
                } else if (all_docs.size() == 1) {
                    // Single document - convert directly to JSON
                    json_str = YAMLNodeToJSON(all_docs[0]);
                } else {
                    // Multiple documents - convert to JSON array
                    json_str = "[";
                    for (size_t i = 0; i < all_docs.size(); i++) {
                        if (i > 0) {
                            json_str += ",";
                        }
                        json_str += YAMLNodeToJSON(all_docs[i]);
                    }
                    json_str += "]";
                }
                
                return StringVector::AddString(result, json_str.c_str(), json_str.length());
            } catch (const std::exception &e) {
                // On error, return NULL
                return string_t();
            }
        });
    
    return true;
}

// Cast from JSON to YAML
static bool JSONToYAMLCast(Vector &source, Vector &result, idx_t count, CastParameters &parameters) {
    UnaryExecutor::Execute<string_t, string_t>(
        source, result, count, [&](string_t json_str) -> string_t {
            if (json_str.GetSize() == 0) {
                return string_t();
            }
            
            try {
                // Parse the JSON using YAML parser
                YAML::Node json_node = YAML::Load(json_str.GetString());
                
                // Configure the emitter for clean YAML output
                YAML::Emitter out;
                out.SetIndent(2);
                out.SetMapFormat(YAML::Block);
                out.SetSeqFormat(YAML::Block);
                
                // Emit as properly formatted YAML
                out << json_node;
                std::string yaml_str = out.c_str();
                
                return StringVector::AddString(result, yaml_str.c_str(), yaml_str.length());
            } catch (const std::exception &e) {
                // On error during conversion, return NULL
                result.SetValue(count, Value(YAMLTypes::YAMLType()));
                return string_t();
            }
        });
    
    return true;
}

// Cast from VARCHAR to YAML
static bool VarcharToYAMLCast(Vector &source, Vector &result, idx_t count, CastParameters &parameters) {
    // Basic pass-through with YAML formatting validation
    UnaryExecutor::Execute<string_t, string_t>(
        source, result, count, [&](string_t str) -> string_t {
            if (str.GetSize() == 0) {
                return string_t();
            }
            
            try {
                // Parse as YAML to validate and normalize
                YAML::Node yaml_node = YAML::Load(str.GetString());
                
                // Format correctly using YAML::Emitter
                YAML::Emitter out;
                out.SetIndent(2);
                out.SetMapFormat(YAML::Block);
                out.SetSeqFormat(YAML::Block);
                
                out << yaml_node;
                std::string yaml_str = out.c_str();
                
                return StringVector::AddString(result, yaml_str.c_str(), yaml_str.length());
            } catch (...) {
                result.SetValue(count, Value(YAMLTypes::YAMLType()));
                return string_t();
            }
        });
    
    return true;
}

// Cast from YAML to VARCHAR
static bool YAMLToVarcharCast(Vector &source, Vector &result, idx_t count, CastParameters &parameters) {
    // Ensure proper YAML formatting for display
    UnaryExecutor::Execute<string_t, string_t>(
        source, result, count, [&](string_t yaml_str) -> string_t {
            if (yaml_str.GetSize() == 0) {
                return string_t();
            }
            
            try {
                // Parse and re-emit with proper formatting
                YAML::Node yaml_node = YAML::Load(yaml_str.GetString());
                
                YAML::Emitter out;
                out.SetIndent(2);
                out.SetMapFormat(YAML::Block);
                out.SetSeqFormat(YAML::Block);
                
                out << yaml_node;
                std::string formatted_yaml = out.c_str();
                
                return StringVector::AddString(result, formatted_yaml.c_str(), formatted_yaml.length());
            } catch (...) {
                // On error, pass through
                return yaml_str;
            }
        });
    
    return true;
}

void YAMLTypes::Register(DatabaseInstance &db) {
    // Get the YAML type (which is VARCHAR)
    auto yaml_type = YAMLType();
    
    // Register the YAML type alias in the catalog
    ExtensionUtil::RegisterType(db, "yaml", yaml_type);
    
    // Register YAML<->JSON cast function
    ExtensionUtil::RegisterCastFunction(db, yaml_type, LogicalType::JSON(), YAMLToJSONCast);
    ExtensionUtil::RegisterCastFunction(db, LogicalType::JSON(), yaml_type, JSONToYAMLCast);

    // Register YAML<->VARCHAR cast function
    ExtensionUtil::RegisterCastFunction(db, LogicalType::VARCHAR, yaml_type, VarcharToYAMLCast);
    ExtensionUtil::RegisterCastFunction(db, yaml_type, LogicalType::VARCHAR, YAMLToVarcharCast);
    
    // Register the scalar functions as before
    auto yaml_to_json_fun = ScalarFunction("yaml_to_json", {yaml_type}, LogicalType::JSON(), YAMLToJSONFunction);
    ExtensionUtil::RegisterFunction(db, yaml_to_json_fun);
    
    auto json_to_yaml_fun = ScalarFunction("json_to_yaml", {LogicalType::JSON()}, yaml_type, JSONToYAMLFunction);
    ExtensionUtil::RegisterFunction(db, json_to_yaml_fun);
    
    auto value_to_yaml_fun = ScalarFunction("value_to_yaml", {LogicalType::ANY}, yaml_type, ValueToYAMLFunction);
    ExtensionUtil::RegisterFunction(db, value_to_yaml_fun);
}

} // namespace duckdb
