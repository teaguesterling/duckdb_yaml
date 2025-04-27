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

namespace duckdb {

inline void YamlScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &name_vector = args.data[0];
    UnaryExecutor::Execute<string_t, string_t>(
	    name_vector, result, args.size(),
	    [&](string_t name) {
			return StringVector::AddString(result, "Yaml "+name.GetString()+" 🐥");
        });
}

inline void YamlOpenSSLVersionScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &name_vector = args.data[0];
    UnaryExecutor::Execute<string_t, string_t>(
	    name_vector, result, args.size(),
	    [&](string_t name) {
			return StringVector::AddString(result, "Yaml " + name.GetString() +
                                                     ", my linked OpenSSL version is " +
                                                     OPENSSL_VERSION_TEXT );
        });
}

static void LoadInternal(DatabaseInstance &instance) {
    // Register YAML reader
    YAMLReader::RegisterFunction(instance);

    // Register a scalar function
    auto yaml_scalar_function = ScalarFunction("yaml", {LogicalType::VARCHAR}, LogicalType::VARCHAR, YamlScalarFun);
    ExtensionUtil::RegisterFunction(instance, yaml_scalar_function);
}

void YamlExtension::Load(DuckDB &db) {
	LoadInternal(*db.instance);
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
