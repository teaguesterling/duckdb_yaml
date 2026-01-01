#pragma once

#include "duckdb.hpp"

namespace duckdb {

class YAMLUnnestFunctions {
public:
	static void Register(ExtensionLoader &loader);
};

} // namespace duckdb
