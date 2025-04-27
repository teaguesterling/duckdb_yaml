#pragma once

#include "duckdb.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/function/table_function.hpp"
#include "yaml-cpp/yaml.h"
#include <vector>

namespace duckdb {

class YAMLReader {
public:
    // Structure to hold YAML read options
    struct YAMLReadOptions {
        bool auto_detect_types = true;
        bool ignore_errors = false;
        size_t maximum_object_size = 16777216;  // 16MB default
        bool multi_document = true;             // Whether to handle multi-document YAML files
        bool expand_root_sequence = true;       // Whether to expand top-level sequences into rows
    };

    // Register the read_yaml table function
    static void RegisterFunction(DatabaseInstance &db);

private:
    // Row-based YAML reading functions
    static unique_ptr<FunctionData> YAMLReadRowsBind(ClientContext &context,
                                                  TableFunctionBindInput &input,
                                                  vector<LogicalType> &return_types,
                                                  vector<string> &names);

    static void YAMLReadRowsFunction(ClientContext &context, TableFunctionInput &data_p,
                                  DataChunk &output);

    // Read YAML from file and convert to DuckDB values
    static unique_ptr<FunctionData> YAMLReadBind(ClientContext &context,
                                              TableFunctionBindInput &input,
                                              vector<LogicalType> &return_types,
                                              vector<string> &names);

    static void YAMLReadFunction(ClientContext &context, TableFunctionInput &data_p,
                              DataChunk &output);

    // Helper to detect logical type from YAML node
    static LogicalType DetectYAMLType(YAML::Node node);

    // Helper to convert YAML node to DuckDB value
    static Value YAMLNodeToValue(YAML::Node node, LogicalType target_type);
};

} // namespace duckdb
