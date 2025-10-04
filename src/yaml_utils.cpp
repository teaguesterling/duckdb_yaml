#include "yaml_utils.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/types/date.hpp"
#include "duckdb/common/types/timestamp.hpp"
#include "duckdb/common/types/time.hpp"
#include "duckdb/function/cast/default_casts.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>

namespace duckdb {

namespace yaml_utils {

//===--------------------------------------------------------------------===//
// YAML Settings Implementation
//===--------------------------------------------------------------------===//

// Initialize default format to FLOW for better compatibility with SQLLogicTest
YAMLFormat YAMLSettings::default_format = YAMLFormat::FLOW;

YAMLFormat YAMLSettings::GetDefaultFormat() {
    return default_format;
}

void YAMLSettings::SetDefaultFormat(YAMLFormat format) {
    default_format = format;
}

//===--------------------------------------------------------------------===//
// YAML Parsing and Emission
//===--------------------------------------------------------------------===//

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

//===--------------------------------------------------------------------===//
// YAML to JSON Conversion
//===--------------------------------------------------------------------===//

// Helper function to check if a string might be a date/timestamp
static bool TryDetectDateOrTimestamp(const std::string& value, std::string& json_value) {
    // Try to parse as date first
    idx_t pos = 0;
    date_t date_result;
    bool special = false;
    
    auto date_cast_result = Date::TryConvertDate(value.c_str(), value.length(), pos, date_result, special, false);
    if (date_cast_result == DateCastResult::SUCCESS && pos == value.length()) {
        // Successfully parsed as date, format it in JSON date format
        json_value = "\"" + Date::ToString(date_result) + "\"";
        return true;
    }
    
    // Try to parse as timestamp
    timestamp_t timestamp_result;
    if (Timestamp::TryConvertTimestamp(value.c_str(), value.length(), timestamp_result, false) == TimestampCastResult::SUCCESS) {
        // Successfully parsed as timestamp, format it in ISO 8601 format with Z suffix for JSON compatibility
        auto timestamp_str = Timestamp::ToString(timestamp_result);
        // Check if timestamp already has timezone info
        if (timestamp_str.find('+') == std::string::npos && timestamp_str.find('Z') == std::string::npos) {
            timestamp_str += "Z"; // Add UTC timezone indicator
        }
        json_value = "\"" + timestamp_str + "\"";
        return true;
    }
    
    // Try to parse as time
    pos = 0;
    dtime_t time_result;
    if (Time::TryConvertTime(value.c_str(), value.length(), pos, time_result, false) && pos == value.length()) {
        // Successfully parsed as time, format it in JSON time format
        json_value = "\"" + Time::ToString(time_result) + "\"";
        return true;
    }
    
    return false;
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
            
            // Check for boolean values (case-insensitive)
            std::string lower_value = value;
            std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
            
            if (lower_value == "true" || lower_value == "yes" || lower_value == "on" || 
                lower_value == "y" || lower_value == "t") {
                return "true";
            } else if (lower_value == "false" || lower_value == "no" || lower_value == "off" || 
                       lower_value == "n" || lower_value == "f") {
                return "false";
            } else if (lower_value == "null" || value == "~" || value == "") {
                return "null";
            }
            
            // Try to parse as number (integer first, then double)
            try {
                // Skip numeric detection for values that look like dates/times
                bool might_be_temporal = false;
                if (value.find('-') != std::string::npos || value.find(':') != std::string::npos) {
                    might_be_temporal = true;
                }
                
                if (!might_be_temporal) {
                    // Check if it's an integer using DuckDB's Value casting
                    try {
                        Value string_val(value);
                        Value int_val = string_val.DefaultCastAs(LogicalType::BIGINT);
                        // If casting succeeded and the result converts back to the same string, it's a valid integer
                        if (int_val.ToString() == value) {
                            return value; // It's an integer, return as is
                        }
                    } catch (...) {
                        // Not a valid integer, continue
                    }
                    
                    // Check if it's a double using DuckDB's Value casting
                    try {
                        Value string_val(value);
                        Value double_val = string_val.DefaultCastAs(LogicalType::DOUBLE);
                        double numeric_val = double_val.GetValue<double>();
                        
                        // Check for special floating point values
                        if (std::isinf(numeric_val)) {
                            return value[0] == '-' ? "\"-Infinity\"" : "\"Infinity\"";
                        } else if (std::isnan(numeric_val)) {
                            return "\"NaN\"";
                        }
                        
                        // Check if the double converts back to the same string representation
                        if (double_val.ToString() == value) {
                            return value; // It's a double, return as is
                        }
                    } catch (...) {
                        // Not a valid double, continue
                    }
                }
            } catch (...) {
                // Not a number, continue with other type detection
            }
            
            // Try to detect date/timestamp/time
            std::string json_value;
            if (TryDetectDateOrTimestamp(value, json_value)) {
                return json_value;
            }
            
            // If all else fails, treat as string and escape JSON special characters
            std::string result = "\"";
            for (char ch : value) {
                switch (ch) {
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
                        if (static_cast<unsigned char>(ch) < MIN_PRINTABLE_CHAR) {
                            char buf[UNICODE_BUFFER_SIZE];
                            snprintf(buf, sizeof(buf), "\\u%04x", ch);
                            result += buf;
                        } else {
                            result += ch;
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

//===--------------------------------------------------------------------===//
// DuckDB Value to YAML Conversion
//===--------------------------------------------------------------------===//

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
                    
                    // Check if it looks like a number using DuckDB's Value casting
                    try {
                        Value string_val(str_val);
                        Value double_val = string_val.DefaultCastAs(LogicalType::DOUBLE);
                        // If casting succeeded, it looks like a number
                        needs_quotes = true;
                    } catch (...) {
                        // Not a number
                    }
                    
                    // Check for special characters
                    for (char ch : str_val) {
                        if (ch == ':' || ch == '{' || ch == '}' || ch == '[' || ch == ']' || 
                            ch == ',' || ch == '&' || ch == '*' || ch == '#' || ch == '?' || 
                            ch == '|' || ch == '-' || ch == '<' || ch == '>' || ch == '=' || 
                            ch == '!' || ch == '%' || ch == '@' || ch == '\\' || ch == '"' ||
                            ch == '\'' || ch == '\n' || ch == '\t' || ch == ' ') {
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

std::string FormatPerStyleAndLayout(const Value& value, YAMLFormat format, const std::string& layout) {
    // Simply return the formatted YAML string - let callers handle layout-specific formatting
    return ValueToYAMLString(value, format);
}

} // namespace yaml_utils

} // namespace duckdb
