#pragma once

#include "duckdb.hpp"

namespace duckdb {

class YAMLFunctions {
public:
    static void Register(ExtensionLoader &loader);

private:
    // Register basic YAML validation function
    static void RegisterValidationFunction(ExtensionLoader &loader);
    
    // Register YAML to JSON conversion function
    static void RegisterConversionFunctions(ExtensionLoader &loader);
    
    // Register YAML type functions (yaml_to_json, value_to_yaml, format_yaml)
    static void RegisterYAMLTypeFunctions(ExtensionLoader &loader);
    
    
    // Register style management functions
    static void RegisterStyleFunctions(ExtensionLoader &loader);
};

} // namespace duckdb
