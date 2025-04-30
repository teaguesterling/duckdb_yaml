#define DUCKDB_EXTENSION_MAIN

#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

// Additional includes for file extension handling
#include "duckdb/main/config.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/tableref/table_function_ref.hpp"

// OpenSSL linked through vcpkg
#include <openssl/opensslv.h>

#include "yaml_extension.hpp"
#include "yaml_reader.hpp"
#include "yaml_functions.hpp"
#include "yaml_types.hpp"

namespace duckdb {

static void LoadInternal(DatabaseInstance &instance) {
    // Register YAML reader
    YAMLReader::RegisterFunction(instance);
    
    // Register YAML functions
    YAMLFunctions::Register(instance);
    
    // Register YAML types
    YAMLTypes::Register(instance);
}

void YamlExtension::Load(DuckDB &db) {
    LoadInternal(*db.instance);
    
    // Register YAML file extension handler
    auto &config = DBConfig::GetConfig(*db.instance);
    
    // Create a file extension handler function for both .yaml and .yml
    auto yaml_reader = [](ClientContext &context, const string &path) -> unique_ptr<TableRef> {
        auto table_function = make_uniq<TableFunctionRef>();
        vector<unique_ptr<ParsedExpression>> function_parameters;
        
        // Create the path parameter
        function_parameters.push_back(make_uniq<ConstantExpression>(Value(path)));
        
        // Set the function name to read_yaml
        table_function->function_name = "read_yaml";
        table_function->expression_parameters = std::move(function_parameters);
        
        return std::move(table_function);
    };
    
    // Register the file extension handlers
    config.RegisterFileExtension("yaml", yaml_reader);
    config.RegisterFileExtension("yml", yaml_reader);
}

std::string YamlExtension::Name() {
	return "yaml";
}

std::string YamlExtension::Version() const {
#ifdef EXT_VERSION_YAML
	return EXT_VERSION_YAML;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_EXTENSION_API void yaml_init(duckdb::DatabaseInstance &db) {
    duckdb::DuckDB db_wrapper(db);
    db_wrapper.LoadExtension<duckdb::YamlExtension>();
}

DUCKDB_EXTENSION_API const char *yaml_version() {
	return duckdb::DuckDB::LibraryVersion();
}
}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
