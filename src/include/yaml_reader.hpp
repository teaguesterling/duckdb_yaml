//===----------------------------------------------------------------------===//
//                         DuckDB
//
// yaml_reader.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include <vector>
#include "duckdb.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/function/cast/cast_function_set.hpp"
#include "duckdb/function/cast/default_casts.hpp"
#include "duckdb/function/replacement_scan.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/parsed_data/create_pragma_function_info.hpp"
#include "duckdb/parser/tableref/table_function_ref.hpp"
#include "yaml-cpp/yaml.h"

namespace duckdb {

class TableRef;
struct ReplacementScanData;

/**
 * @brief Mode for handling multi-document YAML files
 */
enum class MultiDocumentMode {
	ROWS,       // Each document becomes a row (default, same as true)
	FIRST,      // Only first document (same as false)
	FRONTMATTER, // First doc is metadata, rest are data rows
	LIST        // All documents as single row with STRUCT[] column
};

/**
 * @brief YAML Reader class for handling YAML files in DuckDB
 *
 * This class provides functions to read YAML files into DuckDB tables. It supports:
 * - Single files, file lists, glob patterns, and directory paths
 * - Multi-document YAML files
 * - Top-level sequence handling
 * - Robust error recovery options
 * - Type auto-detection
 * - Explicit column type specification
 */
class YAMLReader {

public:
	// Structure to hold YAML read options
	struct YAMLReadOptions {
		bool auto_detect_types = true;         // Whether to auto-detect types from YAML content
		bool ignore_errors = false;            // Whether to ignore parsing errors
		size_t maximum_object_size = 16777216; // 16MB default maximum file size
		MultiDocumentMode multi_document_mode = MultiDocumentMode::ROWS; // How to handle multi-document YAML
		bool expand_root_sequence = true;      // Whether to expand top-level sequences into rows

		// Sampling parameters for schema detection (matching JSON extension behavior)
		idx_t sample_size = STANDARD_VECTOR_SIZE * 10; // Number of rows to sample for schema detection (default: 20480)
		idx_t maximum_sample_files = 32;               // Maximum number of files to sample for schema detection

		// User-specified column types
		vector<string> column_names;      // User-provided column names
		vector<LogicalType> column_types; // User-provided column types

		// Records path for extracting records from nested structure (issue #22)
		string records_path; // Dot-notation path to array of records (e.g., "data.items" or "projects")

		// FRONTMATTER mode options
		bool frontmatter_as_columns = true; // When true, frontmatter fields become columns; when false, single yaml column

		// LIST mode options
		string list_column_name = "documents"; // Column name for the STRUCT[] in LIST mode
	};

	/**
	 * @brief Register the YAML reader functions with DuckDB
	 *
	 * Registers the read_yaml and read_yaml_objects table functions
	 *
	 * @param db The database instance to register the functions with
	 */
	static void RegisterFunction(ExtensionLoader &loader);

	/**
	 * @brief Replace a yaml file string with 'read_yaml'
	 *
	 * Performs a ReadReplacement on any paths containing .yml or .yaml to 'read_yaml'
	 *
	 * @param context Client context for the query
	 * @param input Input expression for replacement
	 * @param data Additional data for the replacement scan
	 */
	static unique_ptr<TableRef> ReadYAMLReplacement(ClientContext &context, ReplacementScanInput &input,
	                                                optional_ptr<ReplacementScanData> data);

private:
	/**
	 * @brief Bind function for read_yaml that expands each document into rows
	 *
	 * Handles file selection, reading, and schema detection. Supports:
	 * - Single files, file lists, glob patterns, and directory paths
	 * - Error handling with ignore_errors parameter
	 * - Auto-detection of column types
	 * - Explicit column type specification
	 *
	 * @param context Client context for the query
	 * @param input Bind input parameters
	 * @param return_types Types of columns to return
	 * @param names Names of columns to return
	 * @return Function data for execution
	 */
	static unique_ptr<FunctionData> YAMLReadRowsBind(ClientContext &context, TableFunctionBindInput &input,
	                                                 vector<LogicalType> &return_types, vector<string> &names);

	/**
	 * @brief Execution function for read_yaml
	 *
	 * @param context Client context
	 * @param input Execution input data
	 * @param state Local state for the function
	 * @param output Output chunk to write results to
	 */
	static void YAMLReadRowsFunction(ClientContext &context, TableFunctionInput &input, DataChunk &output);

	/**
	 * @brief Bind function for read_yaml_objects that returns each document as a column
	 *
	 * @param context Client context for the query
	 * @param input Bind input parameters
	 * @param return_types Types of columns to return
	 * @param names Names of columns to return
	 * @return Function data for execution
	 */
	static unique_ptr<FunctionData> YAMLReadObjectsBind(ClientContext &context, TableFunctionBindInput &input,
	                                                    vector<LogicalType> &return_types, vector<string> &names);

	/**
	 * @brief Execution function for read_yaml_objects
	 *
	 * @param context Client context
	 * @param input Execution input data
	 * @param state Local state for the function
	 * @param output Output chunk to write results to
	 */
	static void YAMLReadObjectsFunction(ClientContext &context, TableFunctionInput &input, DataChunk &output);

	/**
	 * @brief Get file paths from various input types (single file, list, glob, directory)
	 *
	 * @param context Client context for file operations
	 * @param file_path Input file path (can be a single file, list, glob pattern, or directory)
	 * @return vector<string> List of resolved file paths
	 */
	static vector<string> GetFilePaths(ClientContext &context, const string &file_path);

public:
	/**
	 * @brief Get files from a Value (which can be a string or list of strings)
	 *
	 * @param context Client context for file operations
	 * @param path_value The input value containing file path(s)
	 * @param ignore_errors Whether to ignore missing files
	 * @return vector<string> List of resolved file paths
	 */
	static vector<string> GetFiles(ClientContext &context, const Value &path_value, bool ignore_errors);

private:
	/**
	 * @brief Get files from a glob pattern
	 *
	 * @param context Client context for file operations
	 * @param pattern Glob pattern to match files
	 * @return vector<string> List of matching file paths
	 */
	static vector<string> GetGlobFiles(ClientContext &context, const string &pattern);

	/**
	 * @brief Get files from a file list
	 *
	 * @param context Client context for file operations
	 * @param file_path Path to the file containing list of files
	 * @return vector<string> List of file paths from the list
	 */
	static vector<string> GetFileList(ClientContext &context, const string &file_path);

	/**
	 * @brief Detect column schema from a YAML document
	 *
	 * @param node The YAML node to inspect
	 * @param names Output vector of column names
	 * @param types Output vector of column types
	 * @param options Read options
	 */
	static void DetectSchema(const YAML::Node &node, vector<string> &names, vector<LogicalType> &types,
	                         const YAMLReadOptions &options);

public:
	/**
	 * @brief Detect the type of a YAML node
	 *
	 * @param node YAML node to inspect
	 * @return LogicalType The detected DuckDB type
	 */
	static LogicalType DetectYAMLType(const YAML::Node &node);

	/**
	 * @brief Detect YAML type across multiple documents with jagged schema support
	 *
	 * @param nodes Vector of YAML nodes to inspect for merged schema
	 * @return LogicalType The detected DuckDB type with merged fields
	 */
	static LogicalType DetectJaggedYAMLType(const vector<YAML::Node> &nodes);

	/**
	 * @brief Convert a YAML node to a DuckDB value
	 *
	 * @param node YAML node to convert
	 * @param target_type Target type to convert to
	 * @return Value The converted value
	 */
	static Value YAMLNodeToValue(const YAML::Node &node, const LogicalType &target_type);

	/**
	 * @brief Read a YAML file and parse it into documents
	 *
	 * @param context Client context for file operations
	 * @param file_path Path to the YAML file
	 * @param options YAML read options
	 * @return vector<YAML::Node> Parsed YAML documents
	 */
	static vector<YAML::Node> ReadYAMLFile(ClientContext &context, const string &file_path,
	                                       const YAMLReadOptions &options);

	/**
	 * @brief Parse a multi-document YAML file with error recovery
	 *
	 * @param yaml_content The YAML content to parse
	 * @param ignore_errors Whether to attempt error recovery
	 * @return vector<YAML::Node> Successfully parsed documents
	 */
	static vector<YAML::Node> ParseMultiDocumentYAML(const string &yaml_content, bool ignore_errors);

	/**
	 * @brief Extract row nodes from YAML documents
	 *
	 * @param docs The YAML documents to process
	 * @param expand_root_sequence Whether to expand top-level sequences
	 * @return vector<YAML::Node> Row nodes extracted from documents
	 */
	static vector<YAML::Node> ExtractRowNodes(const vector<YAML::Node> &docs, bool expand_root_sequence);

	/**
	 * @brief Process a map node into columnar data
	 *
	 * @param node The map node to process
	 * @param output The output data chunk
	 * @param row_index The current row index
	 * @param names Column names
	 * @param types Column types
	 */
	static void ProcessMapNode(const YAML::Node &node, DataChunk &output, idx_t row_index, const vector<string> &names,
	                           const vector<LogicalType> &types);

	/**
	 * @brief Process a non-map node into columnar data
	 *
	 * @param node The node to process
	 * @param output The output data chunk
	 * @param row_index The current row index
	 * @param column_index The column index
	 * @param current_type The current column type
	 */
	static void ProcessNonMapNode(const YAML::Node &node, DataChunk &output, idx_t row_index, idx_t column_index,
	                              const LogicalType &current_type);

	/**
	 * @brief Attempt to recover partial documents from corrupted YAML
	 *
	 * @param yaml_content The YAML content to recover from
	 * @return vector<YAML::Node> Recovered documents
	 */
	static vector<YAML::Node> RecoverPartialYAMLDocuments(const string &yaml_content);

	/**
	 * @brief Helper function to merge two struct types, preserving fields from both
	 *
	 * @param type1 First struct type
	 * @param type2 Second struct type
	 * @return LogicalType A merged struct with fields from both inputs
	 */
	static LogicalType MergeStructTypes(const LogicalType &type1, const LogicalType &type2);

	/**
	 * @brief Bind the columns parameter for explicit type specification
	 *
	 * @param context Client context
	 * @param input Function bind input
	 * @param options YAML read options to update with column specifications
	 */
	static void BindColumnTypes(ClientContext &context, TableFunctionBindInput &input, YAMLReadOptions &options);

	/**
	 * @brief Navigate to a nested path within a YAML node using dot notation
	 *
	 * @param node The root YAML node to navigate from
	 * @param path Dot-notation path (e.g., "data.items" or "config.users")
	 * @return YAML::Node The node at the specified path, or an undefined node if not found
	 */
	static YAML::Node NavigateToPath(const YAML::Node &node, const string &path);

	/**
	 * @brief Bind function for parse_yaml that parses YAML strings into rows
	 *
	 * @param context Client context for the query
	 * @param input Bind input parameters
	 * @param return_types Types of columns to return
	 * @param names Names of columns to return
	 * @return Function data for execution
	 */
	static unique_ptr<FunctionData> ParseYAMLBind(ClientContext &context, TableFunctionBindInput &input,
	                                              vector<LogicalType> &return_types, vector<string> &names);

	/**
	 * @brief Init function for parse_yaml local state
	 *
	 * @param context Execution context
	 * @param input Init input data
	 * @param global_state Global state (unused)
	 * @return Local state for execution
	 */
	static unique_ptr<LocalTableFunctionState> ParseYAMLInit(ExecutionContext &context, TableFunctionInitInput &input,
	                                                         GlobalTableFunctionState *global_state);

	/**
	 * @brief Execution function for parse_yaml
	 *
	 * @param context Client context
	 * @param input Execution input data
	 * @param output Output chunk to write results to
	 */
	static void ParseYAMLFunction(ClientContext &context, TableFunctionInput &input, DataChunk &output);
};

} // namespace duckdb
