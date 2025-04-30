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

unique_ptr<TableRef> YAMLFunctions::ReadYAMLReplacement(ClientContext &context, ReplacementScanInput &input,
                                                        optional_ptr<ReplacementScanData> data) {
    auto table_name = ReplacementScan::GetFullPath(input);
	if (!ReplacementScan::CanReplace(table_name, {"yaml", "yml"})) {
		return nullptr;
	}
    
	auto table_function = make_uniq<TableFunctionRef>();
	vector<unique_ptr<ParsedExpression>> children;
	children.push_back(make_uniq<ConstantExpression>(Value(table_name)));
	table_function->function = make_uniq<FunctionExpression>("read_yaml", std::move(children));

	if (!FileSystem::HasGlob(table_name)) {
		auto &fs = FileSystem::GetFileSystem(context);
		table_function->alias = fs.ExtractBaseName(table_name);
	}

	return std::move(table_function);
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
    // YAML to JSON conversion function
    ScalarFunction yaml_to_json("yaml_to_json", {LogicalType::VARCHAR}, LogicalType::VARCHAR, 
        [](DataChunk &args, ExpressionState &state, Vector &result) {
            auto &input_vector = args.data[0];
            
            UnaryExecutor::ExecuteWithNulls<string_t, string_t>(input_vector, result, args.size(),
                [&](string_t yaml_str, ValidityMask &mask, idx_t idx) {
                    if (!mask.RowIsValid(idx)) {
                        return string_t();
                    }
                    
                    try {
                        // Convert YAML to JSON using YAMLTypes helper
                        ClientContext &context = state.GetContext();
                        return YAMLTypes::CastYAMLToJSON(context, yaml_str);
                    } catch (const std::exception &e) {
                        throw InvalidInputException("Error converting YAML to JSON: %s", e.what());
                    }
                });
        });
    
    ExtensionUtil::RegisterFunction(db, yaml_to_json);
    
    // JSON to YAML conversion function
    ScalarFunction json_to_yaml("json_to_yaml", {LogicalType::VARCHAR}, LogicalType::VARCHAR, 
        [](DataChunk &args, ExpressionState &state, Vector &result) {
            auto &input_vector = args.data[0];
            
            UnaryExecutor::ExecuteWithNulls<string_t, string_t>(input_vector, result, args.size(),
                [&](string_t json_str, ValidityMask &mask, idx_t idx) {
                    if (!mask.RowIsValid(idx)) {
                        return string_t();
                    }
                    
                    try {
                        // Convert JSON to YAML
                        ClientContext &context = state.GetContext();
                        
                        // Parse JSON and convert to Value
                        Value json_val(json_str);
                        
                        // Convert to YAML
                        return YAMLTypes::CastValueToYAML(context, json_val);
                    } catch (const std::exception &e) {
                        throw InvalidInputException("Error converting JSON to YAML: %s", e.what());
                    }
                });
        });
    
    ExtensionUtil::RegisterFunction(db, json_to_yaml);
}

} // namespace duckdb
