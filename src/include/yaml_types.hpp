#pragma once

#include "duckdb.hpp"
#include "duckdb/common/types.hpp"

namespace duckdb {

class YAMLTypes {
public:
    //! The YAML type used by DuckDB
    static LogicalType YAMLType();
    
    //! Register the YAML type
    static void Register(DatabaseInstance &db);
    
    //! Cast the YAML type to JSON type
    static string_t CastYAMLToJSON(ClientContext &context, string_t yaml_str);
    
    //! Cast a value to the YAML type
    static string_t CastValueToYAML(ClientContext &context, Value value);
};

} // namespace duckdb
