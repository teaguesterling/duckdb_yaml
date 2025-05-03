#pragma once

#include "duckdb.hpp"

namespace duckdb {

class YAMLFunctions {
public:
    static void Register(DatabaseInstance &db);

private:
    // Register basic YAML validation function
    static void RegisterValidationFunction(DatabaseInstance &db);
    
    // Register YAML to JSON conversion function
    static void RegisterConversionFunctions(DatabaseInstance &db);
};

} // namespace duckdb
