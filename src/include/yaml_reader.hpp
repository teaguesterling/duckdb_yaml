#pragma once

#include "duckdb.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/function/table_function.hpp"
#include "yaml-cpp/yaml.h"
#include <vector>

namespace duckdb {

/**
 * @brief YAML Reader class for handling YAML files in DuckDB
 * 
 * This class provides functions to read YAML files into DuckDB tables. It supports:
 * - Single files, file lists, glob patterns, and directory paths
 * - Multi-document YAML files
 * - Top-level sequence handling
 * - Robust error recovery options
 * - Type auto-detection
 */
class YAMLReader {
public:
    // Structure to hold YAML read options
    struct YAMLReadOptions {
        bool auto_detect_types = true;          // Whether to auto-detect types from YAML content
        bool ignore_errors = false;             // Whether to ignore parsing errors
        size_t maximum_object_size = 16777216;  // 16MB default maximum file size
        bool multi_document = true;             // Whether to handle multi-document YAML files
        bool expand_root_sequence = true;       // Whether to expand top-level sequences into rows
    };

    /**
     * @brief Register the YAML reader functions with DuckDB
     * 
     * Registers the read_yaml and read_yaml_objects table functions
     * 
     * @param db The database instance to register the functions with
     */
    static void RegisterFunction(DatabaseInstance &db);

private:
    /**
     * @brief Bind function for read_yaml that expands each document into rows
     * 
     * Handles file selection, reading, and schema detection. Supports:
     * - Single files, file lists, glob patterns, and directory paths
     * - Error handling with ignore_errors parameter
     * - Auto-detection of column types
     * 
     * @param context Client context for the query
     * @param input Bind input parameters
     * @param return_types Types of columns to return
     * @param names Names of columns to return
     * @return Function data for execution
     */
    static unique_ptr<FunctionData> YAMLReadRowsBind(ClientContext &context,
                                                  TableFunctionBindInput &input,
                                                  vector<LogicalType> &return_types,
                                                  vector<string> &names);

    /**
     * @brief Execution function for read_yaml
     * 
     * Processes rows from YAML documents
     * 
     * @param context Client context for the query
     * @param data_p Function data from bind step
     * @param output Output chunk to fill
     */
    static void YAMLReadRowsFunction(ClientContext &context, TableFunctionInput &data_p,
                                  DataChunk &output);

    /**
     * @brief Bind function for read_yaml_objects that preserves document structure
     * 
     * @param context Client context for the query
     * @param input Bind input parameters
     * @param return_types Types of columns to return
     * @param names Names of columns to return
     * @return Function data for execution
     */
    static unique_ptr<FunctionData> YAMLReadBind(ClientContext &context,
                                              TableFunctionBindInput &input,
                                              vector<LogicalType> &return_types,
                                              vector<string> &names);

    /**
     * @brief Execution function for read_yaml_objects
     * 
     * @param context Client context for the query
     * @param data_p Function data from bind step
     * @param output Output chunk to fill
     */
    static void YAMLReadFunction(ClientContext &context, TableFunctionInput &data_p,
                              DataChunk &output);

    /**
     * @brief Detect logical type from YAML node
     * 
     * @param node YAML node to detect type from
     * @return Detected logical type
     */
    static LogicalType DetectYAMLType(const YAML::Node &node);

    /**
     * @brief Convert YAML node to DuckDB value
     * 
     * @param node YAML node to convert
     * @param target_type Target logical type
     * @return Converted DuckDB value
     */
    static Value YAMLNodeToValue(const YAML::Node &node, const LogicalType &target_type);
    
    /**
     * @brief Parse multi-document YAML with error recovery
     * 
     * @param yaml_content YAML content to parse
     * @param ignore_errors Whether to ignore parsing errors
     * @return Vector of valid YAML nodes
     */
    static vector<YAML::Node> ParseMultiDocumentYAML(const string &yaml_content, bool ignore_errors);
    
    /**
     * @brief Recover partial valid documents from YAML with syntax errors
     * 
     * @param yaml_content YAML content to parse
     * @return Vector of valid YAML nodes
     */
    static vector<YAML::Node> RecoverPartialYAMLDocuments(const string &yaml_content);
    
    /**
     * @brief Extract row nodes from YAML documents
     * 
     * @param docs Vector of YAML documents
     * @param expand_root_sequence Whether to expand top-level sequences into rows
     * @return Vector of YAML nodes representing rows
     */
    static vector<YAML::Node> ExtractRowNodes(const vector<YAML::Node> &docs, bool expand_root_sequence);
    
    /**
     * @brief Get files matching a pattern or from a file list
     * 
     * Supports:
     * - Single file paths
     * - Glob patterns (*.yaml, etc.)
     * - File lists
     * - Directory paths (reads all .yaml and .yml files)
     * 
     * @param context Client context for the query
     * @param path_value File path, pattern, or list as a Value
     * @return Vector of matching file paths
     */
    static vector<string> GlobFiles(ClientContext &context, const Value &path_value);
    
    /**
     * @brief Read a single YAML file and parse it
     * 
     * @param context Client context for the query
     * @param file_path Path to the file
     * @param options YAML read options
     * @return Vector of YAML nodes
     */
    static vector<YAML::Node> ReadYAMLFile(ClientContext &context, const string &file_path, 
                                          const YAMLReadOptions &options);
};

} // namespace duckdb
