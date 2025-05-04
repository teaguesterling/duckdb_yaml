#define DUCKDB_EXTENSION_MAIN

#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/main/config.hpp"

#include "yaml_extension.hpp"
#include "yaml_reader.hpp"
//#include "yaml_types.hpp"

namespace duckdb {

static void LoadInternal(DatabaseInstance &instance) {
    // Register YAML reader
    YAMLReader::RegisterFunction(instance);
    
    // Register YAML functions
    //YAMLFunctions::Register(instance);
    
    // Register YAML types
    YAMLTypes::Register(instance);
}

void YamlExtension::Load(DuckDB &db) {
    LoadInternal(*db.instance);
    
    // Register YAML files as automatically recognized by DuckDB
    auto &config = DBConfig::GetConfig(*db.instance);
    
    // Add replacement scan for YAML files
    config.replacement_scans.emplace_back(YAMLReader::ReadYAMLReplacement);
    
    // Also register file extensions using AddExtensionOption for backward compatibility
    //config.AddExtensionOption("yaml", "read_yaml");
    //config.AddExtensionOption("yml", "read_yaml");
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
