#define DUCKDB_EXTENSION_MAIN

#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/function/copy_function.hpp"
#include "duckdb/main/config.hpp"

#include "yaml_extension.hpp"
#include "yaml_reader.hpp"
#include "yaml_types.hpp"
#include "yaml_scalar_functions.hpp"
#include "yaml_extraction_functions.hpp"
#include "yaml_unnest_functions.hpp"

namespace duckdb {

static void LoadInternal(ExtensionLoader &loader) {
    // Register YAML reader
    YAMLReader::RegisterFunction(loader);

    // Register YAML functions
    YAMLFunctions::Register(loader);

    // Register YAML extraction functions
    YAMLExtractionFunctions::Register(loader);

    // Register YAML unnest functions (Phase 1 Core Functions)
    YAMLUnnestFunctions::Register(loader);

    // Register YAML types
    YAMLTypes::Register(loader);

    // Register YAML copy functions
    RegisterYAMLCopyFunctions(loader);

    // Register YAML frontmatter reader
    RegisterYAMLFrontmatterFunction(loader);

    // Register YAML files as automatically recognized by DuckDB
    auto &config = DBConfig::GetConfig(loader.GetDatabaseInstance());
    
    // Add replacement scan for YAML files
    config.replacement_scans.emplace_back(YAMLReader::ReadYAMLReplacement);
}

void YamlExtension::Load(ExtensionLoader &loader) {
    LoadInternal(loader);
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

DUCKDB_CPP_EXTENSION_ENTRY(yaml, loader) {
    duckdb::LoadInternal(loader);
}

DUCKDB_EXTENSION_API const char *yaml_version() {
	return duckdb::DuckDB::LibraryVersion();
}

}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
