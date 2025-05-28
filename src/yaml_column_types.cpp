#include "yaml_reader.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/types.hpp"
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
    
    // Clear any existing column specifications
    options.column_names.clear();
    options.column_types.clear();
    
    // Get child types and values from the struct
    auto &child_types = StructType::GetChildTypes(columns_value.type());
    
    if (child_types.empty()) {
        return; // Empty struct
    }
    
    // Get struct values
    auto &struct_values = StructValue::GetChildren(columns_value);
    
    // For each entry in the struct
    for (idx_t i = 0; i < child_types.size(); i++) {
        string column_name = child_types[i].first;
        Value type_value = struct_values[i];
        
        if (type_value.IsNull()) {
            throw BinderException("Column type for '" + column_name + "' cannot be NULL");
        }
        
        if (type_value.type().id() != LogicalTypeId::VARCHAR) {
            throw BinderException("Column type for '" + column_name + "' must be a string type name");
        }
        
        string type_name = type_value.GetValue<string>();

        // Use DuckDB's built-in type parsing function (same as JSON extension)
        LogicalType col_type;
        try {
            col_type = TransformStringToLogicalType(type_name, context);
        } catch (const Exception &e) {
            throw BinderException("Invalid type '" + type_name + "' for column '" + column_name + "': " + e.what());
        }
        
        options.column_names.push_back(column_name);
        options.column_types.push_back(col_type);
    }
    
    // Note: We don't disable auto-detection here to allow partial column specification
    // where some columns are explicitly typed and others are auto-detected
}

} // namespace duckdb