#pragma once

#include "duckdb.hpp"

namespace duckdb {

class YAMLFunctions {
public:
    static void Register(DatabaseInstance &db);
    static unique_ptr<TableRef> YAMLFunctions::ReadYAMLReplacement(ClientContext &context, ReplacementScanInput &input,
                                                                   optional_ptr<ReplacementScanData> data);

private:
    // Register basic YAML validation function
    static void RegisterValidationFunction(DatabaseInstance &db);
    
    // Register YAML to JSON conversion function
    static void RegisterConversionFunctions(DatabaseInstance &db);
};

} // namespace duckdb
