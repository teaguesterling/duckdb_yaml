#pragma once

#include "duckdb/main/database.hpp"

namespace duckdb {

//! YAML extraction functions similar to JSON functions
class YAMLExtractionFunctions {
public:
    static void Register(DatabaseInstance &db);
};

} // namespace duckdb