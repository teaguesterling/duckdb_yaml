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
    
    // Register YAML type functions (yaml_to_json, value_to_yaml, format_yaml)
    static void RegisterYAMLTypeFunctions(DatabaseInstance &db);
    
    
    // Register style management functions
    static void RegisterStyleFunctions(DatabaseInstance &db);
};

} // namespace duckdb
