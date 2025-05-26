#pragma once

#include "duckdb.hpp"
#include "duckdb/common/types.hpp"

namespace duckdb {

class YAMLTypes {
public:
    //! The YAML type used by DuckDB (implemented as VARCHAR)
    static LogicalType YAMLType();

    //! Check if a logical type is a YAML type
    //static bool IsYAMLType(const LogicalType &t);
    
    //! Register the YAML type and conversion functions
    static void Register(DatabaseInstance &db);
};

} // namespace duckdb
