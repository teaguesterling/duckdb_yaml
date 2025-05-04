#include "yaml_types.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/function/scalar/string_functions.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/function/cast/cast_function_set.hpp"
#include "yaml-cpp/yaml.h"

namespace duckdb {

LogicalType YAMLTypes::YAMLType() {
    // Return VARCHAR type with YAML alias
    auto yaml_type = LogicalType::VARCHAR;
    yaml_type.SetAlias("YAML");
    return yaml_type;
}

// Helper function to convert YAML node to JSON string with proper alias resolution
static std::string YAMLNodeToJSON(const YAML::Node &node, bool resolve_aliases = true) {
    if (!node) {
        return "null";
    }
    
    // If resolving aliases is enabled and this is an alias node,
    // process the node it refers to instead
    YAML::Node target_node = node;
    if (resolve_aliases && node.IsAlias()) {
        target_node = node.Alias();
    }
    
    switch (target_node.Type()) {
        case YAML::NodeType::Null:
            return "null";
        case YAML::NodeType::Scalar: {
            std::string value = target_node.Scalar();
            
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
            for (size_t i = 0; i < target_node.size(); i++) {
                if (i > 0) {
                    result += ",";
                }
                result += YAMLNodeToJSON(target_node[i], resolve_aliases);
            }
            result += "]";
            return result;
        }
        case YAML::NodeType::Map: {
            std::string result = "{";
            bool first = true;
            for (auto it = target_node.begin(); it != target_node.end(); ++it) {
                if (!first) {
                    result += ",";
                }
                first = false;
                
                // Key must be a string in JSON
                std::string key = it->first.Scalar();
                result += "\"" + key + "\":" + YAMLNodeToJSON(it->second, resolve_aliases);
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

// Implementation of CastYAMLToJSON with alias resolution
string_t YAMLTypes::CastYAMLToJSON(ClientContext &context, string_t yaml_str) {
    if (yaml_str.GetSize() == 0) {
        return string_t();
    }
    
    try {
        // Check if the YAML string contains multiple documents
        std::stringstream yaml_stream(yaml_str.GetString());
        std::vector<YAML::Node> all_docs = YAML::LoadAll(yaml_stream);
        
        std::string json_str;
        if (all_docs.size() == 0) {
            json_str = "null";
        } else if (all_docs.size() == 1) {
            // Single document - convert directly to JSON with alias resolution
            json_str = YAMLNodeToJSON(all_docs[0], true);
        } else {
            // Multiple documents - convert to JSON array with alias resolution
            json_str = "[";
            for (size_t i = 0; i < all_docs.size(); i++) {
                if (i > 0) {
                    json_str += ",";
                }
                json_str += YAMLNodeToJSON(all_docs[i], true);
            }
            json_str += "]";
        }
        
        return StringVector::AddString(json_str);
    } catch (const std::exception &e) {
        throw InvalidInputException("Error converting YAML to JSON: %s", e.what());
    }
}

// Implementation of CastValueToYAML using YAML::Emitter
string_t YAMLTypes::CastValueToYAML(ClientContext &context, Value value) {
    std::string yaml_str = ValueToYAMLString(value);
    return StringVector::AddString(yaml_str);
}

// Cast YAML string to JSON with alias resolution
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
                    // Single document - convert directly to JSON with alias resolution
                    std::string json_str = YAMLNodeToJSON(all_docs[0], true);
                    return StringVector::AddString(result, json_str);
                } else {
                    // Multiple documents - convert to JSON array with alias resolution
                    std::string result_str = "[";
                    for (size_t i = 0; i < all_docs.size(); i++) {
                        if (i > 0) {
                            result_str += ",";
                        }
                        result_str += YAMLNodeToJSON(all_docs[i], true);
                    }
                    result_str += "]";
                    return StringVector::AddString(result, result_str);
                }
            } catch (const std::exception &e) {
                throw InvalidInputException("Error converting YAML to JSON: %s", e.what());
            }
        });
}

// Convert Value to YAML using YAML::Emitter
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

// Convert JSON to YAML using YAML::Emitter
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
                // Convert JSON to Value using DuckDB's parser
                Value json_val = Value::INVALID;
                
                // Try to parse JSON
                try {
                    // We could use DuckDB's JSON functions here if available
                    // For now, we'll use a basic approach to convert JSON to Value
                    std::string json_string = json_str.GetString();
                    json_val = Value(json_string);
                } catch (const std::exception &e) {
                    throw InvalidInputException("Error parsing JSON: %s", e.what());
                }
                
                // Convert Value to YAML using YAML::Emitter
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
    auto yaml_to_json_fun = ScalarFunction("yaml_to_json", {yaml_type}, LogicalType::JSON(), YAMLToJSONFunction);
    ExtensionUtil::RegisterFunction(db, yaml_to_json_fun);
    
    // Register the JSON to YAML cast 
    auto json_to_yaml_fun = ScalarFunction("json_to_yaml", {LogicalType::JSON()}, yaml_type, JSONToYAMLFunction);
    ExtensionUtil::RegisterFunction(db, json_to_yaml_fun);
    
    // Register Value to YAML cast
    auto value_to_yaml_fun = ScalarFunction("value_to_yaml", {LogicalType::ANY}, yaml_type, ValueToYAMLFunction);
    ExtensionUtil::RegisterFunction(db, value_to_yaml_fun);
    
    // Register cast from VARCHAR to YAML and from YAML to JSON
    auto &cast_functions = CastFunctionSet::GetCastFunctions(db);
    
    // Register cast from any VARCHAR to YAML
    cast_functions.RegisterCastFunction(LogicalType::VARCHAR, yaml_type, 
        [](Vector &source, Vector &result, idx_t count, CastParameters &parameters) {
            // Simple pass-through cast since YAML is stored as VARCHAR
            VectorOperations::Copy(source, result, count, 0, 0);
            return true;
        }, 0);
    
    // Register cast from YAML to JSON
    cast_functions.RegisterCastFunction(yaml_type, LogicalType::JSON(), 
        [](Vector &source, Vector &result, idx_t count, CastParameters &parameters) {
            // Use our YAML to JSON conversion
            DataChunk input_chunk;
            input_chunk.Initialize(Allocator::DefaultAllocator(), {yaml_type}, 1);
            input_chunk.data[0].Reference(source);
            input_chunk.SetCardinality(count);
            
            ExpressionState state(parameters.context);
            YAMLToJSONFunction(input_chunk, state, result);
            return true;
        }, 10);
}

} // namespace duckdb
