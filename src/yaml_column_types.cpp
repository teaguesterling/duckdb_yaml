#include "yaml_reader.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"

namespace duckdb {

void YAMLReader::BindColumnTypes(ClientContext &context, TableFunctionBindInput &input, 
                               YAMLReadOptions &options) {
    if (input.named_parameters.find("columns") == input.named_parameters.end()) {
        return; // No column specification provided
    }
    
    auto &columns_value = input.named_parameters["columns"];
    
    if (columns_value.IsNull()) {
        return; // NULL columns parameter
    }
    
    if (columns_value.type().id() != LogicalTypeId::STRUCT) {
        throw BinderException("columns parameter must be a struct (e.g., {name: 'VARCHAR', id: 'INTEGER'})");
    }
    
    auto &struct_children = StructValue::GetChildren(columns_value);
    if (struct_children.empty()) {
        return; // Empty struct
    }
    
    // Clear any existing column specifications
    options.column_names.clear();
    options.column_types.clear();
    
    // Extract column names and types from the struct
    for (auto &child : struct_children) {
        string column_name = child.first;
        Value &type_value = child.second;
        
        if (type_value.IsNull()) {
            throw BinderException("Column type for '" + column_name + "' cannot be NULL");
        }
        
        if (type_value.type().id() != LogicalTypeId::VARCHAR) {
            throw BinderException("Column type for '" + column_name + "' must be a string type name");
        }
        
        string type_name = type_value.GetValue<string>();
        LogicalType col_type;
        
        // Parse the type name
        try {
            col_type = LogicalType::FromString(type_name);
        } catch (...) {
            throw BinderException("Invalid type '" + type_name + "' for column '" + column_name + "'");
        }
        
        options.column_names.push_back(column_name);
        options.column_types.push_back(col_type);
    }
    
    // Disable auto-detection when explicit types are provided
    if (!options.column_names.empty()) {
        options.auto_detect_types = false;
    }
}

} // namespace duckdb