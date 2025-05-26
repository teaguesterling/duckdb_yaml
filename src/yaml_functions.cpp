#include "yaml_functions.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "yaml_types.hpp"
#include "yaml-cpp/yaml.h"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/function/replacement_scan.hpp"

namespace duckdb {

void YAMLFunctions::Register(DatabaseInstance &db) {
    // Register validation and basic functions
    RegisterValidationFunction(db);
    RegisterConversionFunctions(db);
}


void YAMLFunctions::RegisterValidationFunction(DatabaseInstance &db) {
    // Basic YAML validity check
    ScalarFunction yaml_valid("yaml_valid", {LogicalType::VARCHAR}, LogicalType::BOOLEAN, 
        [](DataChunk &args, ExpressionState &state, Vector &result) {
            auto &input_vector = args.data[0];
            
            UnaryExecutor::ExecuteWithNulls<string_t, bool>(input_vector, result, args.size(),
                [&](string_t yaml_str, ValidityMask &mask, idx_t idx) {
                    if (!mask.RowIsValid(idx)) {
                        return false;
                    }
                    
                    try {
                        // Check if it's a multi-document YAML by trying to load all documents
                        std::stringstream yaml_stream(yaml_str.GetString());
                        std::vector<YAML::Node> docs = YAML::LoadAll(yaml_stream);
                        return !docs.empty();
                    } catch (...) {
                        return false;
                    }
                });
        });
    
    ExtensionUtil::RegisterFunction(db, yaml_valid);
}

void YAMLFunctions::RegisterConversionFunctions(DatabaseInstance &db) {
    // Conversion functions are already registered in YAMLTypes::Register()
    // This function is kept for future expansion
}

} // namespace duckdb
