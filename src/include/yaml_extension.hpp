#pragma once

#include "duckdb.hpp"
#include "duckdb/function/copy_function.hpp"

namespace duckdb {

const vector<string> yaml_extensions = {"yaml", "yml"};

class YamlExtension : public Extension {
public:
	void Load(ExtensionLoader &loader) override;
	std::string Name() override;
        std::string Version() const override;
};

// YAML copy function
CopyFunction GetYAMLCopyFunction();

// Register YAML copy functions
void RegisterYAMLCopyFunctions(ExtensionLoader &loader);

// Register YAML frontmatter reader function
void RegisterYAMLFrontmatterFunction(ExtensionLoader &loader);

} // namespace duckdb
