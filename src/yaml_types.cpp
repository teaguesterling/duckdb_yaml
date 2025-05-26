#include "yaml_types.hpp"
#include "yaml_debug.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/function/scalar/nested_functions.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/function/scalar/string_functions.hpp"
#include "duckdb/function/function_set.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/common/types/vector.hpp"
#include "yaml-cpp/yaml.h"
#include <algorithm>

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

// Default style management
class YAMLSettings {
public:
    static YAMLFormat GetDefaultFormat();
    static void SetDefaultFormat(YAMLFormat format);
private:
    static YAMLFormat default_format;
};

void ConfigureEmitter(YAML::Emitter& out, YAMLFormat format);
std::string EmitYAML(const YAML::Node& node, YAMLFormat format = YAMLFormat::BLOCK);
std::string EmitYAMLMultiDoc(const std::vector<YAML::Node>& docs, YAMLFormat format = YAMLFormat::BLOCK);

// Value conversion utilities
void EmitValueToYAML(YAML::Emitter& out, const Value& value);
std::string ValueToYAMLString(const Value& value, YAMLFormat format);

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

// Initialize default format to FLOW for better compatibility with SQLLogicTest
YAMLFormat YAMLSettings::default_format = YAMLFormat::FLOW;

YAMLFormat YAMLSettings::GetDefaultFormat() {
    return default_format;
}

void YAMLSettings::SetDefaultFormat(YAMLFormat format) {
    default_format = format;
}

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
            const auto value = node.Scalar();
            
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
                idx_t pos;
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
                    default: {
                        constexpr unsigned char MIN_PRINTABLE_CHAR = 32;
                        constexpr idx_t UNICODE_BUFFER_SIZE = 8;
                        if (static_cast<unsigned char>(c) < MIN_PRINTABLE_CHAR) {
                            char buf[UNICODE_BUFFER_SIZE];
                            snprintf(buf, sizeof(buf), "\\u%04x", c);
                            result += buf;
                        } else {
                            result += c;
                        }
                    }
                }
            }
            result += "\"";
            return result;
        }
        case YAML::NodeType::Sequence: {
            std::string result = "[";
            for (idx_t seq_idx = 0; seq_idx < node.size(); seq_idx++) {
                if (seq_idx > 0) {
                    result += ",";
                }
                result += YAMLNodeToJSON(node[seq_idx]);
            }
            result += "]";
            return result;
        }
        case YAML::NodeType::Map: {
            std::string result = "{";
            bool first = true;
            for (const auto& it : node) {
                if (!first) {
                    result += ",";
                }
                first = false;

                // Key must be a string in JSON
                const auto key = it.first.Scalar();
                result += "\"" + key + "\":" + YAMLNodeToJSON(it.second);
            }
            result += "}";
            return result;
        }
        default:
            throw InternalException("Unknown YAML node type");
    }
}

void ConfigureEmitter(YAML::Emitter& out, YAMLFormat format) {
    constexpr idx_t YAML_INDENT_SIZE = 2;
    out.SetIndent(YAML_INDENT_SIZE);
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
        for (idx_t doc_idx = 0; doc_idx < docs.size(); doc_idx++) {
            if (doc_idx > 0) {
                result += "\n---\n";
            }
            result += EmitYAML(docs[doc_idx], format);
        }
        return result;
    }
}

void EmitValueToYAML(YAML::Emitter& out, const Value& value) {
    try {
        // Handle NULL values within data structures (emit YAML null '~')
        // Note: Top-level NULL inputs are handled at the function level following SQL semantics
        if (value.IsNull()) {
            out << YAML::Null;
            return;
        }
        
        switch (value.type().id()) {
            case LogicalTypeId::VARCHAR: {
                // Check if this is a JSON type (VARCHAR with JSON alias)
                if (value.type().IsJSONType()) {
                    try {
                        // Get the JSON string representation
                        const auto json_str = value.GetValue<string>();

                        // Parse JSON using YAML parser (yaml-cpp can parse JSON)
                        YAML::Node json_node = YAML::Load(json_str);

                        // Emit the parsed structure as YAML
                        out << json_node;
                    } catch (...) {
                        // If JSON parsing fails, emit as quoted string
                        out << YAML::SingleQuoted << value.ToString();
                    }
                    break;
                }

                // Handle regular VARCHAR
                const auto str_val = value.GetValue<string>();

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
                        idx_t pos;
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
                    const auto& list_val = ListValue::GetChildren(value);
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
                    const auto& struct_vals = StructValue::GetChildren(value);
                    const auto& struct_names = StructType::GetChildTypes(value.type());
                    
                    // Safety check for struct children
                    if (struct_vals.size() != struct_names.size()) {
                        throw std::runtime_error("Mismatch between struct values and names");
                    }
                    
                    for (idx_t field_idx = 0; field_idx < struct_vals.size(); field_idx++) {
                        out << YAML::Key << struct_names[field_idx].first;
                        out << YAML::Value;
                        EmitValueToYAML(out, struct_vals[field_idx]);
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

// Convert DuckDB Value to YAML::Node (respects emitter configuration)
YAML::Node ValueToYAMLNode(const Value& value) {
    if (value.IsNull()) {
        return YAML::Node(YAML::NodeType::Null);
    }

    switch (value.type().id()) {
        case LogicalTypeId::VARCHAR: {
            if (value.type().IsJSONType()) {
                try {
                    std::string json_str = value.GetValue<string>();
                    return YAML::Load(json_str);  // Parse JSON as YAML
                } catch (...) {
                    return YAML::Node(value.ToString());
                }
            }
            return YAML::Node(value.ToString());
        }
        case LogicalTypeId::BOOLEAN:
            return YAML::Node(value.GetValue<bool>());
        case LogicalTypeId::TINYINT:
            return YAML::Node(static_cast<int>(value.GetValue<int8_t>()));
        case LogicalTypeId::SMALLINT:
            return YAML::Node(static_cast<int>(value.GetValue<int16_t>()));
        case LogicalTypeId::INTEGER:
            return YAML::Node(value.GetValue<int32_t>());
        case LogicalTypeId::BIGINT:
            return YAML::Node(value.GetValue<int64_t>());
        case LogicalTypeId::FLOAT:
            return YAML::Node(value.GetValue<float>());
        case LogicalTypeId::DOUBLE:
            return YAML::Node(value.GetValue<double>());
        case LogicalTypeId::LIST: {
            YAML::Node list_node(YAML::NodeType::Sequence);
            const auto& list_vals = ListValue::GetChildren(value);
            for (const auto& element : list_vals) {
                list_node.push_back(ValueToYAMLNode(element));  // Recursive
            }
            return list_node;
        }
        case LogicalTypeId::STRUCT: {
            YAML::Node map_node(YAML::NodeType::Map);
            const auto& struct_vals = StructValue::GetChildren(value);
            const auto& struct_names = StructType::GetChildTypes(value.type());

            for (idx_t field_idx = 0; field_idx < struct_vals.size() && field_idx < struct_names.size(); field_idx++) {
                const auto key = struct_names[field_idx].first;
                map_node[key] = ValueToYAMLNode(struct_vals[field_idx]);  // Recursive
            }
            return map_node;
        }
        default:
            return YAML::Node(value.ToString());
    }
}

std::string ValueToYAMLString(const Value& value, YAMLFormat format) {
    try {
        // Convert to YAML::Node first (this respects emitter configuration)
        YAML::Node node = ValueToYAMLNode(value);

        // Now emit with proper format settings
        YAML::Emitter out;
        ConfigureEmitter(out, format);
        out << node;

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
// YAML Function Bind Data
//===--------------------------------------------------------------------===//

struct FormatYAMLBindData : public FunctionData {
    yaml_utils::YAMLFormat style = yaml_utils::YAMLFormat::FLOW;
    string orient = "document";  // document/sequence/objects/mapping
    idx_t indent = 2;
    string quote = "auto";       // auto/always/never/single/double

    unique_ptr<FunctionData> Copy() const override {
        auto result = make_uniq<FormatYAMLBindData>();
        result->style = style;
        result->orient = orient;
        result->indent = indent;
        result->quote = quote;
        return std::move(result);
    }

    bool Equals(const FunctionData &other_p) const override {
        auto &other = other_p.Cast<FormatYAMLBindData>();
        return style == other.style && orient == other.orient &&
               indent == other.indent && quote == other.quote;
    }
};

//===--------------------------------------------------------------------===//
// YAML Named Parameter Bind Function
//===--------------------------------------------------------------------===//

static unique_ptr<FunctionData> FormatYAMLBind(ClientContext &context, ScalarFunction &bound_function,
                                              vector<unique_ptr<Expression>> &arguments) {

    if (arguments.empty()) {
        throw InvalidInputException("format_yaml requires at least one argument");
    }

    // Just validate parameters, don't store custom bind data (following struct_update pattern)

    // Process parameters (arguments beyond the first one)
    for (idx_t i = 1; i < arguments.size(); i++) {
        auto &child = arguments[i];
        string param_name = StringUtil::Lower(child->alias);


        // Check if this is a named parameter (using := syntax)
        if (param_name.empty()) {
            throw BinderException("Need named argument for format_yaml, e.g. style := 'block'");
        }

        // Validate parameter value is constant (following DuckDB pattern)
        if (child->type != ExpressionType::VALUE_CONSTANT) {
            throw BinderException("format_yaml parameter '%s' must be a constant value", param_name);
        }

        // Validate known parameter names
        if (param_name == "style") {
            // Valid parameter, will be processed in execution function
        } else {
            throw BinderException("Unknown parameter '%s' for format_yaml", param_name);
        }
    }

    // Return simple bind data like struct_update does
    return make_uniq<VariableReturnBindData>(LogicalType::VARCHAR);
}

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
                const auto docs = yaml_utils::ParseYAML(yaml_str.GetString(), true);
                
                std::string json_str;
                if (docs.empty()) {
                    json_str = "null";
                } else if (docs.size() == 1) {
                    // Single document - convert directly to JSON
                    json_str = yaml_utils::YAMLNodeToJSON(docs[0]);
                } else {
                    // Multiple documents - convert to JSON array
                    json_str = "[";
                    for (idx_t doc_idx = 0; doc_idx < docs.size(); doc_idx++) {
                        if (doc_idx > 0) {
                            json_str += ",";
                        }
                        json_str += yaml_utils::YAMLNodeToJSON(docs[doc_idx]);
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
                // Use the regular version with flow format for consistent YAML objects
                std::string yaml_str = yaml_utils::ValueToYAMLString(value, yaml_utils::YAMLFormat::FLOW);
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

static void FormatYAMLFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    auto& input = args.data[0];
    yaml_utils::YAMLFormat format = yaml_utils::YAMLSettings::GetDefaultFormat();

    // Access named parameters from the bound function expression (like struct_update)
    auto &func_args = state.expr.Cast<BoundFunctionExpression>().children;

    // Process named parameters - all parameters beyond first must be named (like struct_update)
    for (idx_t arg_idx = 1; arg_idx < func_args.size(); arg_idx++) {
        auto &child = func_args[arg_idx];
        if (child->alias.empty()) {
            throw InvalidInputException("format_yaml requires named parameters, e.g. style := 'block'");
        }

        string param_name = StringUtil::Lower(child->alias);

        if (param_name == "style") {
            // Get the style value from the corresponding argument
            Value style_value = args.data[arg_idx].GetValue(0);
            string style_str = StringUtil::Lower(style_value.ToString());
            if (style_str == "block") {
                format = yaml_utils::YAMLFormat::BLOCK;
            } else if (style_str == "flow") {
                format = yaml_utils::YAMLFormat::FLOW;
            } else {
                throw InvalidInputException("Invalid YAML style '%s'. Valid options are 'flow' or 'block'.", style_str);
            }
        } else {
            throw InvalidInputException("Unknown parameter '%s' for format_yaml", param_name);
        }
        // TODO: Add support for other parameters like orient, indent, quote
    }

    // Process each row
    for (idx_t i = 0; i < args.size(); i++) {
        // Extract the value from the input vector
        Value value = input.GetValue(i);

        // YAML generation using determined parameters
        try {
            if (YAMLDebug::IsDebugModeEnabled()) {
                std::string yaml_str = YAMLDebug::SafeValueToYAMLString(value, format == yaml_utils::YAMLFormat::BLOCK);
                result.SetValue(i, Value(yaml_str));
            } else {
                // Use the regular version with specified parameters
                std::string yaml_str = yaml_utils::ValueToYAMLString(value, format);
                result.SetValue(i, Value(yaml_str));
            }
        } catch (const std::exception& e) {
            // Only catch YAML generation errors, return null for data conversion issues
            result.SetValue(i, Value("null"));
        } catch (...) {
            // For unknown YAML generation exceptions, return null
            result.SetValue(i, Value("null"));
        }
    }
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
                const auto docs = yaml_utils::ParseYAML(yaml_str.GetString(), true);
                
                std::string json_str;
                if (docs.empty()) {
                    json_str = "null";
                } else if (docs.size() == 1) {
                    // Single document - convert directly to JSON
                    json_str = yaml_utils::YAMLNodeToJSON(docs[0]);
                } else {
                    // Multiple documents - convert to JSON array
                    json_str = "[";
                    for (idx_t doc_idx = 0; doc_idx < docs.size(); doc_idx++) {
                        if (doc_idx > 0) {
                            json_str += ",";
                        }
                        json_str += yaml_utils::YAMLNodeToJSON(docs[doc_idx]);
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
                const auto docs = yaml_utils::ParseYAML(yaml_str.GetString(), true);
                
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

// Default style management functions
static void YAMLSetDefaultStyleFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    auto& input = args.data[0];

    for (idx_t i = 0; i < args.size(); i++) {
        Value style_value = input.GetValue(i);

        if (style_value.IsNull()) {
            throw InvalidInputException("YAML style cannot be NULL");
        }

        auto style_str = style_value.ToString();
        std::transform(style_str.begin(), style_str.end(), style_str.begin(), ::tolower);

        if (style_str == "block") {
            yaml_utils::YAMLSettings::SetDefaultFormat(yaml_utils::YAMLFormat::BLOCK);
        } else if (style_str == "flow") {
            yaml_utils::YAMLSettings::SetDefaultFormat(yaml_utils::YAMLFormat::FLOW);
        } else {
            throw InvalidInputException("Invalid YAML style '%s'. Valid options are 'flow' or 'block'.", style_str.c_str());
        }

        result.SetValue(i, Value(style_str));
    }
}

static void YAMLGetDefaultStyleFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    auto current_format = yaml_utils::YAMLSettings::GetDefaultFormat();
    std::string format_name = (current_format == yaml_utils::YAMLFormat::BLOCK) ? "block" : "flow";

    result.SetVectorType(VectorType::CONSTANT_VECTOR);
    auto result_data = ConstantVector::GetData<string_t>(result);
    result_data[0] = StringVector::AddString(result, format_name.c_str(), format_name.length());
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
    
    // Register value_to_yaml function (single parameter, returns YAML type)
    auto value_to_yaml_fun = ScalarFunction("value_to_yaml", {LogicalType::ANY}, yaml_type, ValueToYAMLFunction);
    ExtensionUtil::RegisterFunction(db, value_to_yaml_fun);

    // Register format_yaml function with named parameters (returns VARCHAR for display/formatting)
    auto format_yaml_fun = ScalarFunction("format_yaml", {LogicalType::ANY}, LogicalType::VARCHAR, FormatYAMLFunction, FormatYAMLBind);
    format_yaml_fun.null_handling = FunctionNullHandling::SPECIAL_HANDLING;
    format_yaml_fun.varargs = LogicalType::ANY;  // Allow variable number of arguments
    ExtensionUtil::RegisterFunction(db, format_yaml_fun);

    // Register debug functions
    auto yaml_debug_enable_fun = ScalarFunction("yaml_debug_enable", {}, LogicalType::BOOLEAN, YAMLDebugEnableFunction);
    ExtensionUtil::RegisterFunction(db, yaml_debug_enable_fun);
    
    auto yaml_debug_disable_fun = ScalarFunction("yaml_debug_disable", {}, LogicalType::BOOLEAN, YAMLDebugDisableFunction);
    ExtensionUtil::RegisterFunction(db, yaml_debug_disable_fun);
    
    auto yaml_debug_is_enabled_fun = ScalarFunction("yaml_debug_is_enabled", {}, LogicalType::BOOLEAN, YAMLDebugIsEnabledFunction);
    ExtensionUtil::RegisterFunction(db, yaml_debug_is_enabled_fun);
    
    auto yaml_debug_value_to_yaml_fun = ScalarFunction("yaml_debug_value_to_yaml", {LogicalType::ANY}, yaml_type, YAMLDebugValueToYAMLFunction);
    ExtensionUtil::RegisterFunction(db, yaml_debug_value_to_yaml_fun);

    // Register default style management functions
    auto yaml_set_default_style_fun = ScalarFunction("yaml_set_default_style", {LogicalType::VARCHAR}, LogicalType::VARCHAR, YAMLSetDefaultStyleFunction);
    ExtensionUtil::RegisterFunction(db, yaml_set_default_style_fun);

    auto yaml_get_default_style_fun = ScalarFunction("yaml_get_default_style", {}, LogicalType::VARCHAR, YAMLGetDefaultStyleFunction);
    ExtensionUtil::RegisterFunction(db, yaml_get_default_style_fun);
}

} // namespace duckdb
