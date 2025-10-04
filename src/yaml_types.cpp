#include "yaml_types.hpp"
#include "yaml_utils.hpp"
#include "yaml_formatting.hpp"
#include "yaml-cpp/yaml.h"

namespace duckdb {

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

void YAMLTypes::Register(ExtensionLoader &loader) {
    // Get the YAML type
    auto yaml_type = YAMLType();
    
    // Register the YAML type alias in the catalog
    loader.RegisterType("yaml", yaml_type);

    // Register YAML<->JSON cast functions
    loader.RegisterCastFunction(yaml_type, LogicalType::JSON(), YAMLToJSONCast);
    loader.RegisterCastFunction(LogicalType::JSON(), yaml_type, JSONToYAMLCast);

    // Register YAML<->VARCHAR cast functions
    loader.RegisterCastFunction(LogicalType::VARCHAR, yaml_type, VarcharToYAMLCast);
    loader.RegisterCastFunction(yaml_type, LogicalType::VARCHAR, YAMLToVarcharCast);
}

} // namespace duckdb
