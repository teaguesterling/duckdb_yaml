#include "yaml_extraction_functions.hpp"
#include "yaml_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/common/types/vector.hpp"
#include "duckdb/main/extension_util.hpp"
#include "yaml-cpp/yaml.h"

namespace duckdb {

//===--------------------------------------------------------------------===//
// YAML Path Parsing
//===--------------------------------------------------------------------===//

static vector<string> ParseYAMLPath(const string &path) {
    vector<string> components;
    if (path.empty() || path[0] != '$') {
        throw InvalidInputException("YAML path must start with '$'");
    }
    
    string current_component;
    bool in_quotes = false;
    bool escaped = false;
    
    for (size_t i = 1; i < path.length(); i++) {
        char c = path[i];
        
        if (escaped) {
            current_component += c;
            escaped = false;
            continue;
        }
        
        if (c == '\\') {
            escaped = true;
            continue;
        }
        
        if (c == '\'' || c == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        
        if (!in_quotes && (c == '.' || c == '[')) {
            if (!current_component.empty()) {
                components.push_back(current_component);
                current_component.clear();
            }
            
            if (c == '[') {
                // Handle array index
                size_t j = i + 1;
                while (j < path.length() && path[j] != ']') {
                    j++;
                }
                if (j >= path.length()) {
                    throw InvalidInputException("Unclosed array index in path");
                }
                components.push_back(path.substr(i, j - i + 1));
                i = j;
            }
            continue;
        }
        
        current_component += c;
    }
    
    if (!current_component.empty()) {
        components.push_back(current_component);
    }
    
    return components;
}

static YAML::Node ExtractFromYAML(const YAML::Node &node, const vector<string> &path_components, size_t index = 0) {
    if (index >= path_components.size()) {
        return node;
    }
    
    const string &component = path_components[index];
    
    // Handle array index
    if (!component.empty() && component[0] == '[') {
        if (!node.IsSequence()) {
            return YAML::Node(); // Return null node
        }
        
        string index_str = component.substr(1, component.length() - 2);
        try {
            size_t arr_index = std::stoul(index_str);
            if (arr_index >= node.size()) {
                return YAML::Node();
            }
            return ExtractFromYAML(node[arr_index], path_components, index + 1);
        } catch (...) {
            throw InvalidInputException("Invalid array index: %s", index_str);
        }
    }
    
    // Handle map key
    if (!node.IsMap()) {
        return YAML::Node();
    }
    
    auto child = node[component];
    return ExtractFromYAML(child, path_components, index + 1);
}

//===--------------------------------------------------------------------===//
// YAML Type Functions
//===--------------------------------------------------------------------===//

static void YAMLTypeUnaryFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    UnaryExecutor::Execute<string_t, string_t>(
        args.data[0], result, args.size(),
        [&](string_t yaml_str) -> string_t {
            if (yaml_str.GetSize() == 0) {
                return StringVector::AddString(result, "null", 4);
            }
            
            try {
                YAML::Node node = YAML::Load(yaml_str.GetString());
                string type_str;
                
                switch (node.Type()) {
                    case YAML::NodeType::Null:
                        type_str = "null";
                        break;
                    case YAML::NodeType::Scalar:
                        // Try to detect scalar subtype
                        type_str = "scalar";
                        break;
                    case YAML::NodeType::Sequence:
                        type_str = "array";
                        break;
                    case YAML::NodeType::Map:
                        type_str = "object";
                        break;
                    default:
                        type_str = "undefined";
                        break;
                }
                
                return StringVector::AddString(result, type_str.c_str(), type_str.length());
            } catch (const std::exception &e) {
                throw InvalidInputException("Error parsing YAML: %s", e.what());
            }
        });
}

static void YAMLTypeBinaryFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    BinaryExecutor::ExecuteWithNulls<string_t, string_t, string_t>(
        args.data[0], args.data[1], result, args.size(),
        [&](string_t yaml_str, string_t path_str, ValidityMask &mask, idx_t idx) -> string_t {
            if (yaml_str.GetSize() == 0) {
                return StringVector::AddString(result, "null", 4);
            }
            
            try {
                YAML::Node root = YAML::Load(yaml_str.GetString());
                auto path_components = ParseYAMLPath(path_str.GetString());
                auto node = ExtractFromYAML(root, path_components);
                
                string type_str;
                if (!node) {
                    // Nonexistent path - return SQL NULL
                    mask.SetInvalid(idx);
                    return string_t();
                } else {
                    switch (node.Type()) {
                        case YAML::NodeType::Null:
                            type_str = "null";
                            break;
                        case YAML::NodeType::Scalar:
                            type_str = "scalar";
                            break;
                        case YAML::NodeType::Sequence:
                            type_str = "array";
                            break;
                        case YAML::NodeType::Map:
                            type_str = "object";
                            break;
                        default:
                            type_str = "undefined";
                            break;
                    }
                }
                
                return StringVector::AddString(result, type_str.c_str(), type_str.length());
            } catch (const std::exception &e) {
                throw InvalidInputException("Error in yaml_type: %s", e.what());
            }
        });
}

//===--------------------------------------------------------------------===//
// YAML Extract Functions
//===--------------------------------------------------------------------===//

static void YAMLExtractFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    BinaryExecutor::ExecuteWithNulls<string_t, string_t, string_t>(
        args.data[0], args.data[1], result, args.size(),
        [&](string_t yaml_str, string_t path_str, ValidityMask &mask, idx_t idx) -> string_t {
            if (yaml_str.GetSize() == 0) {
                return StringVector::AddString(result, "null", 4);
            }
            
            try {
                YAML::Node root = YAML::Load(yaml_str.GetString());
                auto path_components = ParseYAMLPath(path_str.GetString());
                auto node = ExtractFromYAML(root, path_components);
                
                // Convert extracted node back to YAML
                if (!node) {
                    // Nonexistent path - return SQL NULL
                    mask.SetInvalid(idx);
                    return string_t();
                }
                
                YAML::Emitter out;
                out.SetIndent(2);
                out.SetMapFormat(YAML::Flow);
                out.SetSeqFormat(YAML::Flow);
                out << node;
                
                string yaml_result = out.c_str();
                return StringVector::AddString(result, yaml_result.c_str(), yaml_result.length());
            } catch (const std::exception &e) {
                throw InvalidInputException("Error in yaml_extract: %s", e.what());
            }
        });
}

static void YAMLExtractStringFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    BinaryExecutor::ExecuteWithNulls<string_t, string_t, string_t>(
        args.data[0], args.data[1], result, args.size(),
        [&](string_t yaml_str, string_t path_str, ValidityMask &mask, idx_t idx) -> string_t {
            if (yaml_str.GetSize() == 0) {
                mask.SetInvalid(idx);
                return string_t();
            }
            
            try {
                YAML::Node root = YAML::Load(yaml_str.GetString());
                auto path_components = ParseYAMLPath(path_str.GetString());
                auto node = ExtractFromYAML(root, path_components);
                
                if (!node) {
                    // Nonexistent path - return SQL NULL
                    mask.SetInvalid(idx);
                    return string_t();
                }

                if (node.IsNull()) {
                    // YAML null value - return SQL NULL (same behavior as JSON)
                    mask.SetInvalid(idx);
                    return string_t();
                }
                
                if (!node.IsScalar()) {
                    // Convert non-scalar to YAML string representation
                    YAML::Emitter out;
                    out.SetIndent(2);
                    out.SetMapFormat(YAML::Flow);
                    out.SetSeqFormat(YAML::Flow);
                    out << node;
                    string yaml_result = out.c_str();
                    return StringVector::AddString(result, yaml_result.c_str(), yaml_result.length());
                }
                
                string str_val = node.Scalar();
                return StringVector::AddString(result, str_val.c_str(), str_val.length());
            } catch (const std::exception &e) {
                throw InvalidInputException("Error in yaml_extract_string: %s", e.what());
            }
        });
}

//===--------------------------------------------------------------------===//
// YAML Exists Function
//===--------------------------------------------------------------------===//

static void YAMLExistsFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    BinaryExecutor::Execute<string_t, string_t, bool>(
        args.data[0], args.data[1], result, args.size(),
        [&](string_t yaml_str, string_t path_str) -> bool {
            if (yaml_str.GetSize() == 0) {
                return false;
            }
            
            try {
                YAML::Node root = YAML::Load(yaml_str.GetString());
                auto path_components = ParseYAMLPath(path_str.GetString());
                auto node = ExtractFromYAML(root, path_components);
                
                return node.IsDefined() && !node.IsNull();
            } catch (...) {
                return false;
            }
        });
}

//===--------------------------------------------------------------------===//
// Registration
//===--------------------------------------------------------------------===//

void YAMLExtractionFunctions::Register(DatabaseInstance &db) {
    // Get the YAML type
    auto yaml_type = YAMLTypes::YAMLType();
    
    // yaml_type function
    ScalarFunctionSet yaml_type_set("yaml_type");
    yaml_type_set.AddFunction(ScalarFunction({yaml_type}, LogicalType::VARCHAR, YAMLTypeUnaryFunction));
    yaml_type_set.AddFunction(ScalarFunction({yaml_type, LogicalType::VARCHAR}, LogicalType::VARCHAR, YAMLTypeBinaryFunction));
    ExtensionUtil::RegisterFunction(db, yaml_type_set);
    
    // yaml_extract function  
    auto yaml_extract_fun = ScalarFunction("yaml_extract", {yaml_type, LogicalType::VARCHAR}, yaml_type, YAMLExtractFunction);
    ExtensionUtil::RegisterFunction(db, yaml_extract_fun);
    
    // yaml_extract_string function
    auto yaml_extract_string_fun = ScalarFunction("yaml_extract_string", {yaml_type, LogicalType::VARCHAR}, LogicalType::VARCHAR, YAMLExtractStringFunction);
    ExtensionUtil::RegisterFunction(db, yaml_extract_string_fun);
    
    // yaml_exists function
    auto yaml_exists_fun = ScalarFunction("yaml_exists", {yaml_type, LogicalType::VARCHAR}, LogicalType::BOOLEAN, YAMLExistsFunction);
    ExtensionUtil::RegisterFunction(db, yaml_exists_fun);
}

} // namespace duckdb