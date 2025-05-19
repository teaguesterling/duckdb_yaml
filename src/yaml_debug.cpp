#include "yaml_debug.hpp"
#include "yaml_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/types/vector.hpp"
#include "duckdb/common/types/value.hpp"

#include <iostream>

namespace duckdb {

// Initialize static members
bool YAMLDebug::debug_mode_enabled = false;
std::function<void(const std::string&)> YAMLDebug::debug_callback = nullptr;

void YAMLDebug::EnableDebugMode() {
    debug_mode_enabled = true;
}

void YAMLDebug::DisableDebugMode() {
    debug_mode_enabled = false;
}

bool YAMLDebug::IsDebugModeEnabled() {
    return debug_mode_enabled;
}

void YAMLDebug::SetDebugCallback(std::function<void(const std::string&)> callback) {
    debug_callback = callback;
}

void YAMLDebug::LogDebug(const std::string& message) {
    // Only try to call the callback if it's explicitly set  
    if (debug_mode_enabled && debug_callback) {
        debug_callback(message);
    }
}

bool YAMLDebug::SafeEmitValueToYAML(YAML::Emitter& out, const Value& value, int depth) {
    // Prevent excessive recursion which could lead to stack overflow
    if (depth > MAX_RECURSION_DEPTH) {
        out << YAML::SingleQuoted << "[Maximum recursion depth exceeded]";
        return true;
    }

    try {
        // Handle null values
        if (value.IsNull()) {
            out << YAML::Null;
            return true;
        }

        // Debug print the type info
        auto type_id = value.type().id();

        
        // Use the exact same logic as the original EmitValueToYAML in yaml_types.cpp
        switch (type_id) {
            case LogicalTypeId::VARCHAR: {
                std::string str_val = value.GetValue<string>();
                
                // Check if the string needs special formatting
                bool needs_quotes = false;
                
                if (str_val.empty()) {
                    needs_quotes = true;
                } else {
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
                            needs_quotes = true;
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
                    out << YAML::SingleQuoted << str_val;
                } else {
                    out << str_val;
                }
                break;
            }
            case LogicalTypeId::BOOLEAN:
                out << value.GetValue<bool>();
                break;
            case LogicalTypeId::TINYINT:
            case LogicalTypeId::SMALLINT:
            case LogicalTypeId::INTEGER:
            case LogicalTypeId::BIGINT:
            case LogicalTypeId::HUGEINT:
            case LogicalTypeId::UTINYINT:
            case LogicalTypeId::USMALLINT:
            case LogicalTypeId::UINTEGER:
            case LogicalTypeId::UBIGINT:
                try {
                    // For all integer types, use ToString to ensure correct output
                    std::string int_str = value.ToString();
                    if (int_str.empty()) {
                        // If conversion fails for some reason, output a default based on type
                        switch (type_id) {
                            case LogicalTypeId::TINYINT:
                                out << value.GetValue<int8_t>();
                                break;
                            case LogicalTypeId::SMALLINT:
                                out << value.GetValue<int16_t>();
                                break;
                            case LogicalTypeId::INTEGER:
                                out << value.GetValue<int32_t>();
                                break;
                            case LogicalTypeId::BIGINT:
                                out << value.GetValue<int64_t>();
                                break;
                            default:
                                out << int_str;
                                break;
                        }
                    } else {
                        // Don't output quotes around numbers
                        out << int_str;
                    }
                } catch (...) {
                    // If conversion fails, use fallback
                    out << YAML::SingleQuoted << value.ToString();
                }
                break;
            case LogicalTypeId::FLOAT:
            case LogicalTypeId::DOUBLE:
            case LogicalTypeId::DECIMAL:
                try {
                    // For floating point types, use ToString for consistency
                    std::string float_str = value.ToString();
                    out << float_str;
                } catch (...) {
                    // If conversion fails, use fallback
                    out << YAML::SingleQuoted << value.ToString();
                }
                break;
            case LogicalTypeId::LIST: {
                try {
                    out << YAML::BeginSeq;
                    auto& list_val = ListValue::GetChildren(value);
                    for (const auto& element : list_val) {
                        SafeEmitValueToYAML(out, element, depth + 1);
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
                        SafeEmitValueToYAML(out, struct_vals[i], depth + 1);
                    }
                    out << YAML::EndMap;
                } catch (...) {
                    // If struct processing fails, emit as string
                    out << YAML::SingleQuoted << value.ToString();
                }
                break;
            }
            default: {
                // For types we don't handle specifically, convert to string
                std::string debug_msg = std::string("[Default case: Type ID ") + 
                    std::to_string(static_cast<int>(type_id)) + 
                    ", Type name: " + value.type().ToString() + 
                    ", Value: " + value.ToString() + "]";
                out << YAML::SingleQuoted << debug_msg;
                break;
            }
        }
        return true;
    } catch (...) {
        // Last-resort fallback - if anything goes wrong, emit null
        try {
            out << YAML::Null;
        } catch (...) {
            // Prevent cascading exceptions
        }
        return false;
    }
}

std::string YAMLDebug::SafeValueToYAMLString(const Value& value, bool flow_format) {
    try {
        YAML::Emitter out;

        // Configure the emitter based on format
        out.SetIndent(2);
        if (flow_format) {
            out.SetMapFormat(YAML::Flow);
            out.SetSeqFormat(YAML::Flow);
        } else {
            out.SetMapFormat(YAML::Block);
            out.SetSeqFormat(YAML::Block);
        }

        // Try to emit the value safely
        if (SafeEmitValueToYAML(out, value, 0)) {
            if (out.good() && out.c_str() != nullptr) {
                std::string result = out.c_str();
                // Don't return empty string as YAML
                if (!result.empty() && result != "") {
                    return result;
                }
            }
        }

        // If emission failed or emitter is in bad state, return null as fallback
        return "null";
    } catch (const std::exception& e) {
        // Handle known exceptions
        return std::string("null /* exception: ") + e.what() + " */";
    } catch (...) {
        // Handle any other unexpected errors
        return "null /* unknown error */";
    }
}

} // namespace duckdb