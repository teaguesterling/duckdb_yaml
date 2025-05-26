#pragma once

#include "duckdb.hpp"
#include "duckdb/function/copy_function.hpp"

namespace duckdb {

const vector<string> yaml_extensions = {"yaml", "yml"};

class YamlExtension : public Extension {
public:
	void Load(DuckDB &db) override;
	std::string Name() override;
        std::string Version() const override;
};

// YAML copy function
CopyFunction GetYAMLCopyFunction();

} // namespace duckdb
