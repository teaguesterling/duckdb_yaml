#pragma once

#include "duckdb.hpp"
#include "duckdb/common/types.hpp"

namespace duckdb {

class YAMLTypes {
public:
    //! The YAML type used by DuckDB (implemented as VARCHAR)
    static LogicalType YAMLType();
    
    //! Register the YAML type and conversion functions
    static void Register(DatabaseInstance &db);
    
    //! Cast the YAML string to JSON string (for use with DuckDB's JSON system)
    static string_t CastYAMLToJSON(ClientContext &context, string_t yaml_str);
    
    //! Cast a value to the YAML string representation
    static string_t CastValueToYAML(ClientContext &context, Value value);
};

} // namespace duckdb
