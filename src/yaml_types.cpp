#include "yaml_types.hpp"
#include "yaml_debug.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/function/scalar/string_functions.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/common/types/vector.hpp"
#include "yaml-cpp/yaml.h"

namespace duckdb {

// Forward declarations for utility functions
namespace yaml_utils {

// YAML parsing utilities
std::vector<YAML::Node> ParseYAML(const std::string& yaml_str, bool multi_doc = true);
std::string YAMLNodeToJSON(const YAML::Node& node);

// YAML emission utilities
enum class YAMLFormat { 
    BLOCK,  // Multi-line, indented format
    FLOW    // Inline, compact format similar to JSON
};

void ConfigureEmitter(YAML::Emitter& out, YAMLFormat format);
std::string EmitYAML(const YAML::Node& node, YAMLFormat format = YAMLFormat::BLOCK);
std::string EmitYAMLMultiDoc(const std::vector<YAML::Node>& docs, YAMLFormat format = YAMLFormat::BLOCK);

// Value conversion utilities
void EmitValueToYAML(YAML::Emitter& out, const Value& value);
std::string ValueToYAMLString(const Value& value, YAMLFormat format = YAMLFormat::BLOCK);

} // namespace yaml_utils

// Forward declaration for debug helper
class YAMLDebug;

//===--------------------------------------------------------------------===//
// YAML Type Definition
//===--------------------------------------------------------------------===//

LogicalType YAMLTypes::YAMLType() {
    auto yaml_type = LogicalType(LogicalTypeId::VARCHAR);
    yaml_type.SetAlias("yaml");
    return yaml_type;
}

static bool IsYAMLType(const LogicalType& t) {
    return t.id() == LogicalTypeId::VARCHAR && t.HasAlias() && t.GetAlias() == "yaml";
}

//===--------------------------------------------------------------------===//
// YAML Utility Implementations
//===--------------------------------------------------------------------===//

namespace yaml_utils {

std::vector<YAML::Node> ParseYAML(const std::string& yaml_str, bool multi_doc) {
    if (yaml_str.empty()) {
        return {};
    }
    
    try {
        std::stringstream yaml_stream(yaml_str);
        if (multi_doc) {
            return YAML::LoadAll(yaml_stream);
        } else {
            std::vector<YAML::Node> result;
            result.push_back(YAML::Load(yaml_stream));
            return result;
        }
    } catch (const std::exception& e) {
        throw InvalidInputException("Error parsing YAML: %s", e.what());
    }
}

std::string YAMLNodeToJSON(const YAML::Node& node) {
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

std::string EmitYAML(const YAML::Node& node, YAMLFormat format) {
    YAML::Emitter out;
    ConfigureEmitter(out, format);
    out << node;
    return out.c_str();
}

std::string EmitYAMLMultiDoc(const std::vector<YAML::Node>& docs, YAMLFormat format) {
    if (docs.empty()) {
        return "";
    }
    
    if (docs.size() == 1) {
        return EmitYAML(docs[0], format);
    }
    
    if (format == YAMLFormat::FLOW) {
        // For inline format, emit as sequence for readability
        YAML::Emitter out;
        ConfigureEmitter(out, format);
        out << YAML::BeginSeq;
        for (const auto& doc : docs) {
            out << doc;
        }
        out << YAML::EndSeq;
        return out.c_str();
    } else {
        // For block format, emit as multiple documents
        std::string result;
        for (size_t i = 0; i < docs.size(); i++) {
            if (i > 0) {
                result += "\n---\n";
            }
            result += EmitYAML(docs[i], format);
        }
        return result;
    }
}

void EmitValueToYAML(YAML::Emitter& out, const Value& value) {
    try {
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
                try {
                    out << value.GetValue<int64_t>();
                } catch (...) {
                    // If casting fails, convert to string as fallback
                    out << YAML::SingleQuoted << value.ToString();
                }
                break;
            case LogicalTypeId::FLOAT:
            case LogicalTypeId::DOUBLE:
                try {
                    out << value.GetValue<double>();
                } catch (...) {
                    // If casting fails, convert to string as fallback
                    out << YAML::SingleQuoted << value.ToString();
                }
                break;
            case LogicalTypeId::LIST: {
                try {
                    out << YAML::BeginSeq;
                    auto& list_val = ListValue::GetChildren(value);
                    for (const auto& element : list_val) {
                        EmitValueToYAML(out, element);
                    }
                    out << YAML::EndSeq;
                } catch (...) {
                    // If list processing fails, emit as string
                    out << YAML::SingleQuoted << value.ToString();
                }
                break;
            }
            case LogicalTypeId::STRUCT: {
                try {
                    out << YAML::BeginMap;
                    auto& struct_vals = StructValue::GetChildren(value);
                    auto& struct_names = StructType::GetChildTypes(value.type());
                    
                    // Safety check for struct children
                    if (struct_vals.size() != struct_names.size()) {
                        throw std::runtime_error("Mismatch between struct values and names");
                    }
                    
                    for (size_t i = 0; i < struct_vals.size(); i++) {
                        out << YAML::Key << struct_names[i].first;
                        out << YAML::Value;
                        EmitValueToYAML(out, struct_vals[i]);
                    }
                    out << YAML::EndMap;
                } catch (...) {
                    // If struct processing fails, emit as string
                    out << YAML::SingleQuoted << value.ToString();
                }
                break;
            }
            default:
                // For types we don't handle specifically, convert to string
                out << YAML::SingleQuoted << value.ToString();
                break;
        }
    } catch (...) {
        // Last-resort fallback - if anything goes wrong, emit null
        try {
            out << YAML::Null;
        } catch (...) {
            // Prevent cascading exceptions
        }
    }
}

std::string ValueToYAMLString(const Value& value, YAMLFormat format) {
    try {
        YAML::Emitter out;
        ConfigureEmitter(out, format);
        EmitValueToYAML(out, value);
        
        // Check if we have a valid YAML string
        if (out.good() && out.c_str() != nullptr) {
            return out.c_str();
        } else {
            // If emitter is in error state, return null as fallback
            return "null";
        }
    } catch (const std::exception& e) {
        // Handle known exceptions
        return "null";
    } catch (...) {
        // Handle any other unexpected errors
        return "null";
    }
}

} // namespace yaml_utils

//===--------------------------------------------------------------------===//
// YAML Vector Functions
//===--------------------------------------------------------------------===//

static void YAMLToJSONFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    UnaryExecutor::Execute<string_t, string_t>(
        args.data[0], result, args.size(),
        [&](string_t yaml_str) -> string_t {
            if (yaml_str.GetSize() == 0) {
                return string_t();
            }
            
            try {
                // Parse as multi-document YAML
                auto docs = yaml_utils::ParseYAML(yaml_str.GetString(), true);
                
                std::string json_str;
                if (docs.empty()) {
                    json_str = "null";
                } else if (docs.size() == 1) {
                    // Single document - convert directly to JSON
                    json_str = yaml_utils::YAMLNodeToJSON(docs[0]);
                } else {
                    // Multiple documents - convert to JSON array
                    json_str = "[";
                    for (size_t i = 0; i < docs.size(); i++) {
                        if (i > 0) {
                            json_str += ",";
                        }
                        json_str += yaml_utils::YAMLNodeToJSON(docs[i]);
                    }
                    json_str += "]";
                }
                
                return StringVector::AddString(result, json_str.c_str(), json_str.length());
            } catch (const std::exception& e) {
                throw InvalidInputException("Error converting YAML to JSON: %s", e.what());
            }
        });
}

static void ValueToYAMLFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    auto& input = args.data[0];

    // Process each row
    for (idx_t i = 0; i < args.size(); i++) {
        try {
            // Extract the value from the input vector
            Value value = input.GetValue(i);

            // If YAMLDebug is enabled, use the safer version
            if (YAMLDebug::IsDebugModeEnabled()) {
                std::string yaml_str = YAMLDebug::SafeValueToYAMLString(value, false);
                result.SetValue(i, Value(yaml_str));
            } else {
                // Use the regular version
                std::string yaml_str = yaml_utils::ValueToYAMLString(value, yaml_utils::YAMLFormat::BLOCK);
                result.SetValue(i, Value(yaml_str));
            }
        } catch (const std::exception& e) {
            // If there's a known exception, return a descriptive message
            result.SetValue(i, Value("null"));
        } catch (...) {
            // For unknown exceptions, just return null
            result.SetValue(i, Value("null"));
        }
    }
}

static void JSONToYAMLFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    UnaryExecutor::Execute<string_t, string_t>(
        args.data[0], result, args.size(),
        [&](string_t json_str) -> string_t {
            if (json_str.GetSize() == 0) {
                return string_t();
            }
            
            try {
                // Parse JSON using YAML parser
                YAML::Node json_node = YAML::Load(json_str.GetString());
                
                // Convert to YAML with block formatting
                std::string yaml_str = yaml_utils::EmitYAML(json_node, yaml_utils::YAMLFormat::BLOCK);
                return StringVector::AddString(result, yaml_str.c_str(), yaml_str.length());
            } catch (const std::exception& e) {
                throw InvalidInputException("Error converting JSON to YAML: %s", e.what());
            }
        });
}

//===--------------------------------------------------------------------===//
// YAML Cast Functions
//===--------------------------------------------------------------------===//

static bool YAMLToJSONCast(Vector& source, Vector& result, idx_t count, CastParameters& parameters) {
    UnaryExecutor::Execute<string_t, string_t>(
        source, result, count, [&](string_t yaml_str) -> string_t {
            if (yaml_str.GetSize() == 0) {
                return string_t();
            }
            
            try {
                // Process as multi-document YAML
                auto docs = yaml_utils::ParseYAML(yaml_str.GetString(), true);
                
                std::string json_str;
                if (docs.empty()) {
                    json_str = "null";
                } else if (docs.size() == 1) {
                    // Single document - convert directly to JSON
                    json_str = yaml_utils::YAMLNodeToJSON(docs[0]);
                } else {
                    // Multiple documents - convert to JSON array
                    json_str = "[";
                    for (size_t i = 0; i < docs.size(); i++) {
                        if (i > 0) {
                            json_str += ",";
                        }
                        json_str += yaml_utils::YAMLNodeToJSON(docs[i]);
                    }
                    json_str += "]";
                }
                
                return StringVector::AddString(result, json_str.c_str(), json_str.length());
            } catch (const std::exception& e) {
                // On error, return empty string
                return string_t();
            }
        });
    
    return true;
}

static bool JSONToYAMLCast(Vector& source, Vector& result, idx_t count, CastParameters& parameters) {
    UnaryExecutor::Execute<string_t, string_t>(
        source, result, count, [&](string_t json_str) -> string_t {
            if (json_str.GetSize() == 0) {
                return string_t();
            }
            
            try {
                // Parse JSON using YAML parser
                YAML::Node json_node = YAML::Load(json_str.GetString());
                
                // Generate YAML with block formatting
                std::string yaml_str = yaml_utils::EmitYAML(json_node, yaml_utils::YAMLFormat::BLOCK);
                return StringVector::AddString(result, yaml_str.c_str(), yaml_str.length());
            } catch (const std::exception& e) {
                // On error, return empty string
                return string_t();
            }
        });
    
    return true;
}

static bool VarcharToYAMLCast(Vector& source, Vector& result, idx_t count, CastParameters& parameters) {
    UnaryExecutor::Execute<string_t, string_t>(
        source, result, count, [&](string_t str) -> string_t {
            if (str.GetSize() == 0) {
                return string_t();
            }
            
            try {
                // Try to parse as multi-document YAML first
                auto docs = yaml_utils::ParseYAML(str.GetString(), true);
                
                // Format as block-style YAML
                std::string yaml_str = yaml_utils::EmitYAMLMultiDoc(docs, yaml_utils::YAMLFormat::BLOCK);
                return StringVector::AddString(result, yaml_str.c_str(), yaml_str.length());
            } catch (...) {
                // On parsing error, return empty string
                return string_t();
            }
        });
    
    return true;
}

static bool YAMLToVarcharCast(Vector& source, Vector& result, idx_t count, CastParameters& parameters) {
    UnaryExecutor::Execute<string_t, string_t>(
        source, result, count, [&](string_t yaml_str) -> string_t {
            if (yaml_str.GetSize() == 0) {
                return string_t();
            }
            
            try {
                // Parse as multi-document YAML
                auto docs = yaml_utils::ParseYAML(yaml_str.GetString(), true);
                
                // Format using inline (flow) style for display purposes
                std::string formatted_yaml = yaml_utils::EmitYAMLMultiDoc(docs, yaml_utils::YAMLFormat::FLOW);
                return StringVector::AddString(result, formatted_yaml.c_str(), formatted_yaml.length());
            } catch (...) {
                // On error, pass through the original string
                return yaml_str;
            }
        });
    
    return true;
}

//===--------------------------------------------------------------------===//
// Registration
//===--------------------------------------------------------------------===//

// Debug mode scalar functions
static void YAMLDebugEnableFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    YAMLDebug::EnableDebugMode();
    result.SetVectorType(VectorType::CONSTANT_VECTOR);
    ConstantVector::GetData<bool>(result)[0] = true;
}

static void YAMLDebugDisableFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    YAMLDebug::DisableDebugMode();
    result.SetVectorType(VectorType::CONSTANT_VECTOR);
    ConstantVector::GetData<bool>(result)[0] = false;
}

static void YAMLDebugIsEnabledFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    result.SetVectorType(VectorType::CONSTANT_VECTOR);
    ConstantVector::GetData<bool>(result)[0] = YAMLDebug::IsDebugModeEnabled();
}

static void YAMLDebugValueToYAMLFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    auto& input = args.data[0];

    // Process each row
    for (idx_t i = 0; i < args.size(); i++) {
        try {
            // Extract the value from the input vector
            Value value = input.GetValue(i);

            std::string yaml_str = YAMLDebug::SafeValueToYAMLString(value, false);
            result.SetValue(i, Value(yaml_str));
        } catch (const std::exception& e) {
            // If there's a known exception, return a descriptive message
            std::string error_msg = "Error converting to YAML: " + std::string(e.what());
            result.SetValue(i, Value(error_msg));
        } catch (...) {
            // For unknown exceptions, just return null
            result.SetValue(i, Value("null"));
        }
    }
}

void YAMLTypes::Register(DatabaseInstance& db) {
    // Get the YAML type
    auto yaml_type = YAMLType();
    
    // Register the YAML type alias in the catalog
    ExtensionUtil::RegisterType(db, "yaml", yaml_type);
    
    // Register YAML<->JSON cast functions
    ExtensionUtil::RegisterCastFunction(db, yaml_type, LogicalType::JSON(), YAMLToJSONCast);
    ExtensionUtil::RegisterCastFunction(db, LogicalType::JSON(), yaml_type, JSONToYAMLCast);

    // Register YAML<->VARCHAR cast functions
    ExtensionUtil::RegisterCastFunction(db, LogicalType::VARCHAR, yaml_type, VarcharToYAMLCast);
    ExtensionUtil::RegisterCastFunction(db, yaml_type, LogicalType::VARCHAR, YAMLToVarcharCast);
    
    // Register scalar functions
    auto yaml_to_json_fun = ScalarFunction("yaml_to_json", {yaml_type}, LogicalType::JSON(), YAMLToJSONFunction);
    ExtensionUtil::RegisterFunction(db, yaml_to_json_fun);
    
    auto json_to_yaml_fun = ScalarFunction("json_to_yaml", {LogicalType::JSON()}, yaml_type, JSONToYAMLFunction);
    ExtensionUtil::RegisterFunction(db, json_to_yaml_fun);
    
    auto value_to_yaml_fun = ScalarFunction("value_to_yaml", {LogicalType::ANY}, yaml_type, ValueToYAMLFunction);
    ExtensionUtil::RegisterFunction(db, value_to_yaml_fun);
    
    // Register debug functions
    auto yaml_debug_enable_fun = ScalarFunction("yaml_debug_enable", {}, LogicalType::BOOLEAN, YAMLDebugEnableFunction);
    ExtensionUtil::RegisterFunction(db, yaml_debug_enable_fun);
    
    auto yaml_debug_disable_fun = ScalarFunction("yaml_debug_disable", {}, LogicalType::BOOLEAN, YAMLDebugDisableFunction);
    ExtensionUtil::RegisterFunction(db, yaml_debug_disable_fun);
    
    auto yaml_debug_is_enabled_fun = ScalarFunction("yaml_debug_is_enabled", {}, LogicalType::BOOLEAN, YAMLDebugIsEnabledFunction);
    ExtensionUtil::RegisterFunction(db, yaml_debug_is_enabled_fun);
    
    auto yaml_debug_value_to_yaml_fun = ScalarFunction("yaml_debug_value_to_yaml", {LogicalType::ANY}, yaml_type, YAMLDebugValueToYAMLFunction);
    ExtensionUtil::RegisterFunction(db, yaml_debug_value_to_yaml_fun);
}

} // namespace duckdb