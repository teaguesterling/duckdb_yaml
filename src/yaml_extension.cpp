#define DUCKDB_EXTENSION_MAIN

#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

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

    // Register a scalar function
    auto yaml_scalar_function = ScalarFunction("yaml", {LogicalType::VARCHAR}, LogicalType::VARCHAR, YamlScalarFun);
    ExtensionUtil::RegisterFunction(instance, yaml_scalar_function);
    
    // Register OpenSSL version function
    auto yaml_openssl_version = ScalarFunction("yaml_openssl_version", {LogicalType::VARCHAR}, LogicalType::VARCHAR, 
                                             YamlOpenSSLVersionScalarFun);
    ExtensionUtil::RegisterFunction(instance, yaml_openssl_version);
}

void YamlExtension::Load(DuckDB &db) {
    LoadInternal(*db.instance);
    
    // Register file extensions for automatic handling with TableFunction
    auto &fs = FileSystem::GetFileSystem(*db.instance);
    
    // Register .yaml and .yml extensions to use read_yaml function
    fs.RegisterSubstrait("yaml", "read_yaml");
    fs.RegisterSubstrait("yml", "read_yaml");
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
