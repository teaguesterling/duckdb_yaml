#include "yaml_reader.hpp"
#include "duckdb/catalog/catalog_entry/table_function_catalog_entry.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include "duckdb/main/extension_util.hpp"
#include <unordered_set>

namespace duckdb {

// Helper function to merge two struct types, preserving fields from both
// This is crucial for handling nested properties that might exist in some documents but not others
// For example, if document1 has {user: {profile: {name: "John"}}} and 
// document2 has {user: {profile: {name: "Jane", age: 42}}},
// we need to make sure the final schema includes both name and age in the profile struct
LogicalType YAMLReader::MergeStructTypes(const LogicalType &type1, const LogicalType &type2) {
	if (type1.id() != LogicalTypeId::STRUCT || type2.id() != LogicalTypeId::STRUCT) {
		return LogicalType::VARCHAR; // Fallback if either is not a struct
	}

	// Get child types from both structs
	auto struct1_children = StructType::GetChildTypes(type1);
	auto struct2_children = StructType::GetChildTypes(type2);

	// Start with all children from the first struct
	child_list_t<LogicalType> merged_children = struct1_children;

	// Add any new fields from the second struct
	for (const auto &child2 : struct2_children) {
		bool found = false;
		for (size_t i = 0; i < merged_children.size(); i++) {
			if (merged_children[i].first == child2.first) {
				// Field exists in both structs, recursively merge if both are structs
				if (merged_children[i].second.id() == LogicalTypeId::STRUCT && 
				    child2.second.id() == LogicalTypeId::STRUCT) {
					merged_children[i].second = MergeStructTypes(merged_children[i].second, child2.second);
				} else if (merged_children[i].second.id() != child2.second.id()) {
					// Different types, default to VARCHAR
					merged_children[i].second = LogicalType::VARCHAR;
				}
				found = true;
				break;
			}
		}
		
		// Add new field if not found in first struct
		if (!found) {
			merged_children.push_back(child2);
		}
	}

	return LogicalType::STRUCT(merged_children);
}

// Bind data structure for read_yaml
struct YAMLReadRowsBindData : public TableFunctionData {
	YAMLReadRowsBindData(string file_path, YAMLReader::YAMLReadOptions options)
	    : file_path(std::move(file_path)), options(options) {
	}

	string file_path;
	YAMLReader::YAMLReadOptions options;
	vector<YAML::Node> yaml_docs; // Each document in the YAML file
	vector<string> names;         // Column names
	vector<LogicalType> types;    // Column types
	idx_t current_doc = 0;        // Current document being processed
};

// Bind data structure for read_yaml_objects
struct YAMLReadBindData : public TableFunctionData {
	YAMLReadBindData(string file_path, YAMLReader::YAMLReadOptions options)
	    : file_path(std::move(file_path)), options(options) {
	}

	string file_path;
	YAMLReader::YAMLReadOptions options;
	vector<YAML::Node> yaml_docs;
	vector<string> names;
	vector<LogicalType> types;
	idx_t current_row = 0;
};

unique_ptr<FunctionData> YAMLReader::YAMLReadRowsBind(ClientContext &context, TableFunctionBindInput &input,
                                                      vector<LogicalType> &return_types, vector<string> &names) {
	// Validate primary input
	if (input.inputs.empty()) {
		throw BinderException("read_yaml requires a file path parameter");
	}

	// Extract the file path parameter
	Value path_value = input.inputs[0];
	string file_path;

	// For display purposes and bind data, we'll use a string representation
	if (path_value.type().id() == LogicalTypeId::VARCHAR) {
		file_path = path_value.ToString();
	} else if (path_value.type().id() == LogicalTypeId::LIST) {
		// For lists, use the first file for the bind data's file_path (just for reference)
		auto &file_list = ListValue::GetChildren(path_value);
		if (!file_list.empty()) {
			file_path = file_list[0].ToString() + " and others";
		} else {
			throw BinderException("File list cannot be empty");
		}
	} else {
		throw BinderException("File path must be a string or list of strings");
	}

	YAMLReadOptions options;

	// Check for duplicate parameters
	std::unordered_set<std::string> seen_parameters;
	for (auto &param : input.named_parameters) {
		if (seen_parameters.find(param.first) != seen_parameters.end()) {
			throw BinderException("Duplicate parameter name: " + param.first);
		}
		seen_parameters.insert(param.first);
	}

	// Check for columns parameter
	if (seen_parameters.find("columns") != seen_parameters.end()) {
		// Bind column types
		BindColumnTypes(context, input, options);
	}

	// Parse optional parameters
	if (seen_parameters.find("auto_detect") != seen_parameters.end()) {
		options.auto_detect_types = input.named_parameters["auto_detect"].GetValue<bool>();
	}
	if (seen_parameters.find("ignore_errors") != seen_parameters.end()) {
		options.ignore_errors = input.named_parameters["ignore_errors"].GetValue<bool>();
	}
	if (seen_parameters.find("maximum_object_size") != seen_parameters.end()) {
		options.maximum_object_size = input.named_parameters["maximum_object_size"].GetValue<int64_t>();
		if (options.maximum_object_size <= 0) {
			throw BinderException("maximum_object_size must be a positive integer");
		}
	}
	if (seen_parameters.find("multi_document") != seen_parameters.end()) {
		options.multi_document = input.named_parameters["multi_document"].GetValue<bool>();
	}
	if (seen_parameters.find("expand_root_sequence") != seen_parameters.end()) {
		options.expand_root_sequence = input.named_parameters["expand_root_sequence"].GetValue<bool>();
	}

	// Create bind data
	auto result = make_uniq<YAMLReadRowsBindData>(file_path, options);

	// Get files using value processing
	auto files = GetFiles(context, path_value, options.ignore_errors);
	if (files.empty() && !options.ignore_errors) {
		throw IOException("No YAML files found matching the input path");
	}

	// Vector to store all the row data items (documents or elements)
	vector<YAML::Node> row_nodes;

	// Read and process all files
	for (const auto &file_path : files) {
		try {
			auto docs = ReadYAMLFile(context, file_path, options);
			auto file_nodes = ExtractRowNodes(docs, options.expand_root_sequence);

			// Add nodes from this file
			row_nodes.insert(row_nodes.end(), file_nodes.begin(), file_nodes.end());
		} catch (const std::exception &e) {
			if (!options.ignore_errors) {
				throw IOException("Error processing YAML file '" + file_path + "': " + string(e.what()));
			}
			// With ignore_errors=true, we allow continuing with other files
		}
	}

	// Replace the docs with our processed row_nodes
	result->yaml_docs = row_nodes;

	// Handle empty result set early
	// TODO: This is very messy and could probably be drastically simplified
	// or at the very least, moved into a helper function
	if (result->yaml_docs.empty()) {
		if (options.ignore_errors) {
			// With ignore_errors=true, return an empty table with a dummy structure
			// that matches what would be expected if data existed
			if (path_value.type().id() == LogicalTypeId::LIST) {
				// For file lists, try to infer schema from files that exist
				for (const auto &file_val : ListValue::GetChildren(path_value)) {
					string file = file_val.ToString();
					if (FileSystem::GetFileSystem(context).FileExists(file)) {
						// Try to read this file to infer schema
						try {
							auto docs = ReadYAMLFile(context, file, options);
							if (!docs.empty() && docs[0].IsMap()) {
								// Use the first valid document to define columns
								for (auto it = docs[0].begin(); it != docs[0].end(); ++it) {
									string key = it->first.Scalar();
									LogicalType type;
									if (options.auto_detect_types) {
										type = DetectYAMLType(it->second);
									} else {
										type = LogicalType::VARCHAR;
									}
									names.push_back(key);
									return_types.push_back(type);
								}
								break;
							}
						} catch (...) {
							// Ignore errors when inferring schema
						}
					}
				}
			} else if (path_value.type().id() == LogicalTypeId::VARCHAR) {
				// For a single file/pattern, try to infer from that
				try {
					string file = path_value.ToString();
					// If it's a glob pattern, try to find the first matching file
					if (file.find('*') != string::npos || file.find('?') != string::npos) {
						auto globbed = FileSystem::GetFileSystem(context).Glob(file);
						if (!globbed.empty()) {
							file = globbed[0].path;
						}
					}

					if (FileSystem::GetFileSystem(context).FileExists(file)) {
						auto docs = ReadYAMLFile(context, file, options);
						if (!docs.empty() && docs[0].IsMap()) {
							// Use the first valid document to define columns
							for (auto it = docs[0].begin(); it != docs[0].end(); ++it) {
								string key = it->first.Scalar();
								LogicalType type;
								if (options.auto_detect_types) {
									type = DetectYAMLType(it->second);
								} else {
									type = LogicalType::VARCHAR;
								}
								names.push_back(key);
								return_types.push_back(type);
							}
						}
					}
				} catch (...) {
					// Ignore errors when inferring schema
				}
			}

			// If we still don't have columns, add a dummy column
			if (names.empty()) {
				names.emplace_back("yaml");
				return_types.emplace_back(LogicalType::VARCHAR);
			}
		} else {
			// Without ignore_errors, this is an error
			throw IOException("No valid YAML documents found");
		}

		// Save schema and return the result with empty data
		result->names = names;
		result->types = return_types;
		return std::move(result);
	}

	// Extract schema from all row nodes, considering user-provided column types
	// Use a map to track column types
	unordered_map<string, LogicalType> user_specified_types;
	unordered_map<string, LogicalType> detected_types;

	// Store user-specified column types
	for (size_t idx = 0; idx < options.column_names.size() && idx < options.column_types.size(); idx++) {
		user_specified_types[options.column_names[idx]] = options.column_types[idx];
	}

	// Process each row node to detect schema from YAML document order
	vector<string> column_order;
	unordered_set<string> seen_columns;

	for (auto &node : result->yaml_docs) {
		// Process each top-level key in document order
		for (auto it = node.begin(); it != node.end(); ++it) {
			std::string key = it->first.Scalar();
			YAML::Node value = it->second;

			// Track column order from first document
			if (seen_columns.find(key) == seen_columns.end()) {
				column_order.push_back(key);
				seen_columns.insert(key);
			}

			// Check if user specified a type for this column
			auto user_type_it = user_specified_types.find(key);
			if (user_type_it != user_specified_types.end()) {
				// User specified type takes precedence
				detected_types[key] = user_type_it->second;
			} else if (detected_types.find(key) == detected_types.end()) {
				// Auto-detect type for new columns
				LogicalType value_type;
				if (options.auto_detect_types) {
					value_type = DetectYAMLType(value);
				} else {
					value_type = LogicalType::VARCHAR;
				}
				detected_types[key] = value_type;
			} else {
				// Reconcile types for existing auto-detected columns
				LogicalType value_type;
				if (options.auto_detect_types) {
					value_type = DetectYAMLType(value);
				} else {
					value_type = LogicalType::VARCHAR;
				}

				// Special handling for struct types to merge nested properties
				// This fixes issues where nested fields like 'profile.age' might only exist in some documents
				// Without this logic, fields that aren't present in the first document would be missing from the schema
				if (detected_types[key].id() == LogicalTypeId::STRUCT && value_type.id() == LogicalTypeId::STRUCT) {
					// Merge the two struct definitions recursively to preserve all fields
					detected_types[key] = MergeStructTypes(detected_types[key], value_type);
				} else if (detected_types[key].id() != value_type.id()) {
					detected_types[key] = LogicalType::VARCHAR;
				}
			}
		}
	}

	// Build the final schema in document order
	for (const auto &col : column_order) {
		names.push_back(col);
		return_types.push_back(detected_types[col]);
	}

	// Special handling for non-map documents
	if (names.empty() && !result->yaml_docs.empty()) {
		// This could happen with non-map documents without expand_root_sequence
		// Add a fallback value column
		names.emplace_back("value");
		if (options.auto_detect_types) {
			return_types.emplace_back(DetectYAMLType(result->yaml_docs[0]));
		} else {
			return_types.emplace_back(LogicalType::VARCHAR);
		}
	}

	// Save the schema
	result->names = names;
	result->types = return_types;

	return std::move(result);
}

unique_ptr<FunctionData> YAMLReader::YAMLReadObjectsBind(ClientContext &context, TableFunctionBindInput &input,
                                                         vector<LogicalType> &return_types, vector<string> &names) {
	// Validate primary input
	if (input.inputs.empty()) {
		throw BinderException("read_yaml_objects requires a file path parameter");
	}

	// Extract the file path parameter
	Value path_value = input.inputs[0];
	string file_path;

	// For display purposes and bind data, we'll use a string representation
	if (path_value.type().id() == LogicalTypeId::VARCHAR) {
		file_path = path_value.ToString();
	} else if (path_value.type().id() == LogicalTypeId::LIST) {
		// For lists, use the first file for the bind data's file_path (just for reference)
		auto &file_list = ListValue::GetChildren(path_value);
		if (!file_list.empty()) {
			file_path = file_list[0].ToString() + " and others";
		} else {
			throw BinderException("File list cannot be empty");
		}
	} else {
		throw BinderException("File path must be a string or list of strings");
	}

	YAMLReadOptions options;

	// Check for duplicate parameters
	std::unordered_set<std::string> seen_parameters;
	for (auto &param : input.named_parameters) {
		if (seen_parameters.find(param.first) != seen_parameters.end()) {
			throw BinderException("Duplicate parameter name: " + param.first);
		}
		seen_parameters.insert(param.first);
	}

	// Check for columns parameter
	if (seen_parameters.find("columns") != seen_parameters.end()) {
		// Bind column types
		BindColumnTypes(context, input, options);
	}

	// Parse optional parameters
	if (seen_parameters.find("auto_detect") != seen_parameters.end()) {
		options.auto_detect_types = input.named_parameters["auto_detect"].GetValue<bool>();
	}
	if (seen_parameters.find("ignore_errors") != seen_parameters.end()) {
		options.ignore_errors = input.named_parameters["ignore_errors"].GetValue<bool>();
	}
	if (seen_parameters.find("maximum_object_size") != seen_parameters.end()) {
		options.maximum_object_size = input.named_parameters["maximum_object_size"].GetValue<int64_t>();
		if (options.maximum_object_size <= 0) {
			throw BinderException("maximum_object_size must be a positive integer");
		}
	}
	if (seen_parameters.find("multi_document") != seen_parameters.end()) {
		options.multi_document = input.named_parameters["multi_document"].GetValue<bool>();
	}
	if (seen_parameters.find("expand_root_sequence") != seen_parameters.end()) {
		options.expand_root_sequence = input.named_parameters["expand_root_sequence"].GetValue<bool>();
	}

	// Create bind data
	auto result = make_uniq<YAMLReadBindData>(file_path, options);

	// Get files using value processing
	auto files = GetFiles(context, path_value, options.ignore_errors);
	if (files.empty() && !options.ignore_errors) {
		throw IOException("No YAML files found matching the input path");
	}

	// Vector to store all YAML documents
	vector<YAML::Node> all_docs;

	// Read and process all files
	for (const auto &file_path : files) {
		try {
			auto docs = ReadYAMLFile(context, file_path, options);
			all_docs.insert(all_docs.end(), docs.begin(), docs.end());
		} catch (const std::exception &e) {
			if (!options.ignore_errors) {
				throw IOException("Error processing YAML file '" + file_path + "': " + string(e.what()));
			}
			// With ignore_errors=true, we allow continuing with other files
		}
	}

	// Store all documents
	result->yaml_docs = all_docs;

	if (result->yaml_docs.empty()) {
		// Empty result - use user-specified columns if available
		if (!options.column_names.empty()) {
			names = options.column_names;
			return_types = options.column_types;
		} else {
			names.emplace_back("yaml");
			return_types.emplace_back(LogicalType::VARCHAR);
		}
		return std::move(result);
	}

	// Use user-specified columns if provided, otherwise auto-detect
	if (!options.column_names.empty()) {
		names = options.column_names;
		return_types = options.column_types;
	} else {
		// Auto-detect columns from first document
		if (options.auto_detect_types) {
			// For each document, we create a column with the document structure
			names.emplace_back("yaml");
			auto doc_type = DetectYAMLType(result->yaml_docs[0]);
			return_types.emplace_back(doc_type);
		} else {
			// If not auto-detecting, just use VARCHAR for the whole document
			names.emplace_back("yaml");
			return_types.emplace_back(LogicalType::VARCHAR);
		}
	}

	// Save column info
	result->names = names;
	result->types = return_types;

	return std::move(result);
}

void YAMLReader::YAMLReadRowsFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = (YAMLReadRowsBindData &)*data_p.bind_data;

	// If we've processed all rows, we're done
	if (bind_data.current_doc >= bind_data.yaml_docs.size()) {
		output.SetCardinality(0);
		return;
	}

	// Process up to STANDARD_VECTOR_SIZE rows at a time
	idx_t count = 0;
	idx_t max_count = std::min((idx_t)STANDARD_VECTOR_SIZE, bind_data.yaml_docs.size() - bind_data.current_doc);

	// Set up the output chunk
	output.Reset();

	// Special case: if we have a dummy column due to ignore_errors=true, just return empty result
	if (bind_data.names.size() == 1 &&
	    (bind_data.names[0] == "yaml" && bind_data.types[0].id() == LogicalTypeId::STRUCT &&
	     StructType::GetChildTypes(bind_data.types[0]).empty())) {
		// Just return empty result for dummy columns with no data
		output.SetCardinality(0);
		bind_data.current_doc = bind_data.yaml_docs.size(); // Mark as completed
		return;
	}

	// Handle value column specially (non-map documents)
	if (bind_data.names.size() == 1 && bind_data.names[0] == "value") {
		for (idx_t doc_idx = 0; doc_idx < max_count; doc_idx++) {
			// Get the current YAML node
			YAML::Node node = bind_data.yaml_docs[bind_data.current_doc + doc_idx];

			// Convert to DuckDB value
			Value val = YAMLNodeToValue(node, bind_data.types[0]);

			// Add to output
			output.SetValue(0, count, val);
			count++;
		}
	} else {
		// Normal case - process map documents
		for (idx_t doc_idx = 0; doc_idx < max_count; doc_idx++) {
			// Get the current YAML node
			YAML::Node node = bind_data.yaml_docs[bind_data.current_doc + doc_idx];

			// Process each column
			for (idx_t col_idx = 0; col_idx < bind_data.names.size(); col_idx++) {
				string &name = bind_data.names[col_idx];
				LogicalType &type = bind_data.types[col_idx];

				// Get the value for this column
				YAML::Node value = node[name];

				// Convert to DuckDB value
				Value duckdb_value;
				if (value) {
					duckdb_value = YAMLNodeToValue(value, type);
				} else {
					duckdb_value = Value(type); // NULL value
				}

				// Add to output
				output.SetValue(col_idx, count, duckdb_value);
			}
			count++;
		}
	}

	// Update current row
	bind_data.current_doc += count;

	// Set the cardinality
	output.SetCardinality(count);
}

void YAMLReader::YAMLReadObjectsFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = (YAMLReadBindData &)*data_p.bind_data;

	// If we've processed all rows, we're done
	if (bind_data.current_row >= bind_data.yaml_docs.size()) {
		output.SetCardinality(0);
		return;
	}

	// Process up to STANDARD_VECTOR_SIZE rows at a time
	idx_t count = 0;
	idx_t max_count = std::min((idx_t)STANDARD_VECTOR_SIZE, bind_data.yaml_docs.size() - bind_data.current_row);

	// Set up the output chunk
	output.Reset();

	// Fill the output
	for (idx_t doc_idx = 0; doc_idx < max_count; doc_idx++) {
		// Get the current YAML node
		YAML::Node node = bind_data.yaml_docs[bind_data.current_row + doc_idx];

		// Convert to DuckDB value
		Value val = YAMLNodeToValue(node, bind_data.types[0]);

		// Add to output
		output.SetValue(0, count, val);
		count++;
	}

	// Update current row
	bind_data.current_row += count;

	// Set the cardinality
	output.SetCardinality(count);
}

} // namespace duckdb