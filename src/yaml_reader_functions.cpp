#include "yaml_reader.hpp"
#include "yaml_types.hpp"
#include "duckdb/catalog/catalog_entry/table_function_catalog_entry.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include <unordered_set>

namespace duckdb {

// Helper function to parse multi_document parameter (bool or string) into MultiDocumentMode
static MultiDocumentMode ParseMultiDocumentMode(const Value &value) {
	if (value.type().id() == LogicalTypeId::BOOLEAN) {
		return value.GetValue<bool>() ? MultiDocumentMode::ROWS : MultiDocumentMode::FIRST;
	}

	if (value.type().id() == LogicalTypeId::VARCHAR) {
		string mode_str = StringUtil::Lower(value.ToString());
		if (mode_str == "rows" || mode_str == "true") {
			return MultiDocumentMode::ROWS;
		} else if (mode_str == "first" || mode_str == "false") {
			return MultiDocumentMode::FIRST;
		} else if (mode_str == "frontmatter") {
			return MultiDocumentMode::FRONTMATTER;
		} else if (mode_str == "list") {
			return MultiDocumentMode::LIST;
		} else {
			throw BinderException("Invalid multi_document mode '%s'. Valid values are: true, false, 'rows', 'first', "
			                      "'frontmatter', 'list'",
			                      mode_str);
		}
	}

	throw BinderException("multi_document parameter must be a boolean or string");
}

// Helper function to merge two struct types, preserving fields from both
// This is crucial for handling nested properties that might exist in some documents but not others
// For example, if document1 has {user: {profile: {name: "John"}}} and
// document2 has {user: {profile: {name: "Jane", age: 42}}},
// we need to make sure the final schema includes both name and age in the profile struct
LogicalType YAMLReader::MergeStructTypes(const LogicalType &type1, const LogicalType &type2) {
	if (type1.id() != LogicalTypeId::STRUCT || type2.id() != LogicalTypeId::STRUCT) {
		// Type conflict (e.g., STRUCT vs scalar) - fall back to YAML to preserve data
		// This matches the JSON extension's behavior (which falls back to JSON type)
		return YAMLTypes::YAMLType();
	}

	// Get child types from both structs
	auto struct1_children = StructType::GetChildTypes(type1);
	auto struct2_children = StructType::GetChildTypes(type2);

	// Handle empty structs (issue #33): empty maps {} create STRUCT() with no children
	// When merging with a non-empty struct, return the non-empty one
	// This allows empty YAML maps to coexist with populated maps in lists/documents
	if (struct1_children.empty()) {
		return type2;
	}
	if (struct2_children.empty()) {
		return type1;
	}

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
				} else if (merged_children[i].second.id() == LogicalTypeId::LIST &&
				           child2.second.id() == LogicalTypeId::LIST) {
					// Both are lists - merge the child types if they are structs
					auto merged_child = ListType::GetChildType(merged_children[i].second);
					auto child2_child = ListType::GetChildType(child2.second);
					if (merged_child.id() == LogicalTypeId::STRUCT && child2_child.id() == LogicalTypeId::STRUCT) {
						merged_children[i].second = LogicalType::LIST(MergeStructTypes(merged_child, child2_child));
					} else if (merged_child.id() != child2_child.id()) {
						// Different child types - fall back to YAML list
						merged_children[i].second = LogicalType::LIST(YAMLTypes::YAMLType());
					}
				} else if (merged_children[i].second.id() != child2.second.id()) {
					// Different types within struct fields - fall back to YAML to preserve data
					merged_children[i].second = YAMLTypes::YAMLType();
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
	vector<YAML::Node> yaml_docs; // Each document in the YAML file (or data rows for FRONTMATTER)
	vector<string> names;         // Column names
	vector<LogicalType> types;    // Column types
	idx_t current_doc = 0;        // Current document being processed

	// FRONTMATTER mode: metadata from the first document
	YAML::Node frontmatter;                // First document (metadata) for FRONTMATTER mode
	vector<string> frontmatter_names;      // Frontmatter column names
	vector<LogicalType> frontmatter_types; // Frontmatter column types
	vector<Value> frontmatter_values;      // Frontmatter values (repeated for each row)

	// LIST mode: all documents in a single row
	bool list_mode_done = false; // Track if we've already returned the single row
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
		options.multi_document_mode = ParseMultiDocumentMode(input.named_parameters["multi_document"]);
	}
	if (seen_parameters.find("expand_root_sequence") != seen_parameters.end()) {
		options.expand_root_sequence = input.named_parameters["expand_root_sequence"].GetValue<bool>();
	}
	if (seen_parameters.find("frontmatter_as_columns") != seen_parameters.end()) {
		options.frontmatter_as_columns = input.named_parameters["frontmatter_as_columns"].GetValue<bool>();
	}
	if (seen_parameters.find("list_column_name") != seen_parameters.end()) {
		options.list_column_name = input.named_parameters["list_column_name"].GetValue<string>();
		if (options.list_column_name.empty()) {
			throw BinderException("list_column_name cannot be empty");
		}
	}
	if (seen_parameters.find("sample_size") != seen_parameters.end()) {
		auto arg = input.named_parameters["sample_size"].GetValue<int64_t>();
		if (arg == -1) {
			options.sample_size = NumericLimits<idx_t>::Maximum();
		} else if (arg > 0) {
			options.sample_size = static_cast<idx_t>(arg);
		} else {
			throw BinderException("read_yaml \"sample_size\" parameter must be positive, or -1 to sample all input");
		}
	}
	if (seen_parameters.find("maximum_sample_files") != seen_parameters.end()) {
		auto arg = input.named_parameters["maximum_sample_files"].GetValue<int64_t>();
		if (arg == -1) {
			options.maximum_sample_files = NumericLimits<idx_t>::Maximum();
		} else if (arg > 0) {
			options.maximum_sample_files = static_cast<idx_t>(arg);
		} else {
			throw BinderException(
			    "read_yaml \"maximum_sample_files\" parameter must be positive, or -1 to remove the limit");
		}
	}
	if (seen_parameters.find("records") != seen_parameters.end()) {
		options.records_path = input.named_parameters["records"].GetValue<string>();
		if (options.records_path.empty()) {
			throw BinderException("read_yaml \"records\" parameter cannot be an empty string");
		}
		// When using records path, we don't expand root sequences (the records path points to the sequence)
		options.expand_root_sequence = false;
	}
	if (seen_parameters.find("strip_document_suffixes") != seen_parameters.end()) {
		options.strip_document_suffixes = input.named_parameters["strip_document_suffixes"].GetValue<bool>();
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
	// Vector to store all documents (for LIST mode and FRONTMATTER mode)
	vector<YAML::Node> all_docs;
	// Vector for schema detection sampling (limited by sample_size and maximum_sample_files)
	vector<YAML::Node> sample_nodes;
	idx_t sampled_rows = 0;
	idx_t sampled_files = 0;

	// Read and process all files
	for (const auto &current_file : files) {
		try {
			auto docs = ReadYAMLFile(context, current_file, options);

			// For LIST and FRONTMATTER modes, we need all documents
			if (options.multi_document_mode == MultiDocumentMode::LIST ||
			    options.multi_document_mode == MultiDocumentMode::FRONTMATTER) {
				all_docs.insert(all_docs.end(), docs.begin(), docs.end());
			}

			vector<YAML::Node> file_nodes;

			// If records path is specified, extract records from that path
			if (!options.records_path.empty()) {
				for (const auto &doc : docs) {
					YAML::Node records_node = NavigateToPath(doc, options.records_path);
					// Check if path was found - check for Undefined type or null node
					if (records_node.Type() == YAML::NodeType::Undefined ||
					    records_node.Type() == YAML::NodeType::Null || !records_node.IsDefined()) {
						if (!options.ignore_errors) {
							throw BinderException("Records path '" + options.records_path +
							                      "' not found in YAML document");
						}
						continue; // Skip this document
					}
					if (!records_node.IsSequence()) {
						if (!options.ignore_errors) {
							throw BinderException("Records path '" + options.records_path +
							                      "' does not point to a sequence/array");
						}
						continue; // Skip this document
					}
					// Extract each element from the sequence as a row
					for (size_t idx = 0; idx < records_node.size(); idx++) {
						if (records_node[idx].IsMap()) {
							file_nodes.push_back(records_node[idx]);
						}
					}
				}
			} else if (options.multi_document_mode == MultiDocumentMode::ROWS ||
			           options.multi_document_mode == MultiDocumentMode::FIRST) {
				// Use the standard extraction logic for ROWS and FIRST modes
				file_nodes = ExtractRowNodes(docs, options.expand_root_sequence);
			}
			// Note: FRONTMATTER and LIST modes handle documents differently (below)

			// Add nodes from this file to the full result set (for ROWS/FIRST modes)
			if (options.multi_document_mode == MultiDocumentMode::ROWS ||
			    options.multi_document_mode == MultiDocumentMode::FIRST) {
				row_nodes.insert(row_nodes.end(), file_nodes.begin(), file_nodes.end());

				// Add nodes to sample set if we haven't reached the sampling limits
				if (sampled_files < options.maximum_sample_files && sampled_rows < options.sample_size) {
					for (const auto &node : file_nodes) {
						if (sampled_rows >= options.sample_size) {
							break;
						}
						sample_nodes.push_back(node);
						sampled_rows++;
					}
					sampled_files++;
				}
			}
		} catch (const std::exception &e) {
			if (!options.ignore_errors) {
				throw IOException("Error processing YAML file '" + current_file + "': " + string(e.what()));
			}
			// With ignore_errors=true, we allow continuing with other files
		}
	}

	// Mode-specific processing
	if (options.multi_document_mode == MultiDocumentMode::FRONTMATTER) {
		// FRONTMATTER mode: first document is metadata, rest are data rows
		if (all_docs.size() < 2) {
			if (!options.ignore_errors) {
				throw BinderException("FRONTMATTER mode requires at least 2 documents (frontmatter + data)");
			}
			// With ignore_errors, treat as empty result
		} else {
			// Store frontmatter (first document)
			result->frontmatter = all_docs[0];

			// Extract data rows from remaining documents
			vector<YAML::Node> data_docs(all_docs.begin() + 1, all_docs.end());
			row_nodes = ExtractRowNodes(data_docs, options.expand_root_sequence);

			// Sample from data rows
			for (const auto &node : row_nodes) {
				if (sampled_rows >= options.sample_size) {
					break;
				}
				sample_nodes.push_back(node);
				sampled_rows++;
			}
		}
	} else if (options.multi_document_mode == MultiDocumentMode::LIST) {
		// LIST mode: all documents as a single row with STRUCT[] column
		// We don't use row_nodes here; we process all_docs directly
		// sample_nodes should contain all docs for schema detection
		for (const auto &doc : all_docs) {
			if (sampled_rows >= options.sample_size) {
				break;
			}
			sample_nodes.push_back(doc);
			sampled_rows++;
		}
	}

	// Replace the docs with our processed row_nodes (not used for LIST mode)
	result->yaml_docs = row_nodes;

	// Handle LIST mode separately - all documents in a single row
	if (options.multi_document_mode == MultiDocumentMode::LIST) {
		// Detect merged schema from all documents
		LogicalType list_element_type = DetectJaggedYAMLType(sample_nodes);

		// Create a LIST type of the detected struct type
		names.push_back(options.list_column_name);
		return_types.push_back(LogicalType::LIST(list_element_type));

		// Store all docs for execution (we'll return them as a single row)
		result->yaml_docs.clear();
		for (const auto &doc : all_docs) {
			result->yaml_docs.push_back(doc);
		}

		result->names = names;
		result->types = return_types;
		return std::move(result);
	}

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

	// Extract schema from sampled row nodes, considering user-provided column types
	// Use a map to track column types
	unordered_map<string, LogicalType> user_specified_types;
	unordered_map<string, LogicalType> detected_types;

	// Store user-specified column types
	for (size_t idx = 0; idx < options.column_names.size() && idx < options.column_types.size(); idx++) {
		user_specified_types[options.column_names[idx]] = options.column_types[idx];
	}

	// Process sampled row nodes to detect schema from YAML document order
	// Uses sample_nodes (limited by sample_size and maximum_sample_files) for schema detection
	vector<string> column_order;
	unordered_set<string> seen_columns;

	for (auto &node : sample_nodes) {
		// Process each top-level key in document order
		for (auto it = node.begin(); it != node.end(); ++it) {
			std::string key = it->first.Scalar();
			YAML::Node value = it->second;

			// Track column order from first document
			if (seen_columns.find(key) == seen_columns.end()) {
				column_order.push_back(key);
				seen_columns.insert(key);
			}

			// Always preserve dots and slashes in struct field names
			// This ensures correct handling of property names like "example.com/my-property"

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
					// Type conflict - fall back to YAML type to preserve data
					detected_types[key] = YAMLTypes::YAMLType();
				}
			}
		}
	}

	// Handle FRONTMATTER mode - add frontmatter columns before data columns
	if (options.multi_document_mode == MultiDocumentMode::FRONTMATTER && result->frontmatter.IsDefined()) {
		if (options.frontmatter_as_columns) {
			// Add frontmatter fields as separate columns with "meta_" prefix
			if (result->frontmatter.IsMap()) {
				for (auto it = result->frontmatter.begin(); it != result->frontmatter.end(); ++it) {
					string key = "meta_" + it->first.Scalar();
					LogicalType type;
					if (options.auto_detect_types) {
						type = DetectYAMLType(it->second);
					} else {
						type = LogicalType::VARCHAR;
					}
					result->frontmatter_names.push_back(key);
					result->frontmatter_types.push_back(type);
					result->frontmatter_values.push_back(YAMLNodeToValue(it->second, type));

					// Add to output schema (frontmatter columns come first)
					names.push_back(key);
					return_types.push_back(type);
				}
			}
		} else {
			// Add frontmatter as a single YAML column
			names.push_back("frontmatter");
			return_types.push_back(YAMLTypes::YAMLType());
			result->frontmatter_names.push_back("frontmatter");
			result->frontmatter_types.push_back(YAMLTypes::YAMLType());
			result->frontmatter_values.push_back(YAMLNodeToValue(result->frontmatter, YAMLTypes::YAMLType()));
		}
	}

	// Build the final schema in document order (data columns)
	for (const auto &col : column_order) {
		names.push_back(col);
		return_types.push_back(detected_types[col]);
	}

	// Special handling for non-map documents
	if (names.empty() && !sample_nodes.empty()) {
		// This could happen with non-map documents without expand_root_sequence
		// Add a fallback value column
		names.emplace_back("value");
		if (options.auto_detect_types) {
			return_types.emplace_back(DetectYAMLType(sample_nodes[0]));
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
		options.multi_document_mode = ParseMultiDocumentMode(input.named_parameters["multi_document"]);
	}
	if (seen_parameters.find("expand_root_sequence") != seen_parameters.end()) {
		options.expand_root_sequence = input.named_parameters["expand_root_sequence"].GetValue<bool>();
	}
	if (seen_parameters.find("sample_size") != seen_parameters.end()) {
		auto arg = input.named_parameters["sample_size"].GetValue<int64_t>();
		if (arg == -1) {
			options.sample_size = NumericLimits<idx_t>::Maximum();
		} else if (arg > 0) {
			options.sample_size = static_cast<idx_t>(arg);
		} else {
			throw BinderException(
			    "read_yaml_objects \"sample_size\" parameter must be positive, or -1 to sample all input");
		}
	}
	if (seen_parameters.find("maximum_sample_files") != seen_parameters.end()) {
		auto arg = input.named_parameters["maximum_sample_files"].GetValue<int64_t>();
		if (arg == -1) {
			options.maximum_sample_files = NumericLimits<idx_t>::Maximum();
		} else if (arg > 0) {
			options.maximum_sample_files = static_cast<idx_t>(arg);
		} else {
			throw BinderException(
			    "read_yaml_objects \"maximum_sample_files\" parameter must be positive, or -1 to remove the limit");
		}
	}
	if (seen_parameters.find("strip_document_suffixes") != seen_parameters.end()) {
		options.strip_document_suffixes = input.named_parameters["strip_document_suffixes"].GetValue<bool>();
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
	// Vector for schema detection sampling (limited by sample_size and maximum_sample_files)
	vector<YAML::Node> sample_docs;
	idx_t sampled_rows = 0;
	idx_t sampled_files = 0;

	// Read and process all files
	for (const auto &file_path : files) {
		try {
			auto docs = ReadYAMLFile(context, file_path, options);
			all_docs.insert(all_docs.end(), docs.begin(), docs.end());

			// Add docs to sample set if we haven't reached the sampling limits
			if (sampled_files < options.maximum_sample_files && sampled_rows < options.sample_size) {
				for (const auto &doc : docs) {
					if (sampled_rows >= options.sample_size) {
						break;
					}
					sample_docs.push_back(doc);
					sampled_rows++;
				}
				sampled_files++;
			}
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
		// Auto-detect columns from sampled documents
		if (options.auto_detect_types) {
			// For each document, we create a column with the document structure
			names.emplace_back("yaml");
			// Use jagged schema detection on sampled docs for metadata compatibility
			auto doc_type = DetectJaggedYAMLType(sample_docs);
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

	// Handle LIST mode - return all documents as a single row with STRUCT[] column
	if (bind_data.options.multi_document_mode == MultiDocumentMode::LIST) {
		if (bind_data.list_mode_done) {
			output.SetCardinality(0);
			return;
		}

		output.Reset();

		// Convert all documents to a list of values
		vector<Value> doc_values;
		LogicalType element_type;
		if (bind_data.types[0].id() == LogicalTypeId::LIST) {
			element_type = ListType::GetChildType(bind_data.types[0]);
		} else {
			element_type = bind_data.types[0];
		}

		for (const auto &doc : bind_data.yaml_docs) {
			doc_values.push_back(YAMLNodeToValue(doc, element_type));
		}

		// Create the list value
		Value list_value = Value::LIST(element_type, doc_values);
		output.SetValue(0, 0, list_value);

		bind_data.list_mode_done = true;
		output.SetCardinality(1);
		return;
	}

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
		// Normal case - process map documents (and FRONTMATTER mode)
		idx_t frontmatter_col_count = bind_data.frontmatter_values.size();

		for (idx_t doc_idx = 0; doc_idx < max_count; doc_idx++) {
			// Get the current YAML node
			YAML::Node node = bind_data.yaml_docs[bind_data.current_doc + doc_idx];

			idx_t col_idx = 0;

			// For FRONTMATTER mode, first add frontmatter columns (same value for each row)
			if (bind_data.options.multi_document_mode == MultiDocumentMode::FRONTMATTER) {
				for (idx_t fm_idx = 0; fm_idx < frontmatter_col_count; fm_idx++) {
					output.SetValue(col_idx, count, bind_data.frontmatter_values[fm_idx]);
					col_idx++;
				}
			}

			// Process data columns
			for (; col_idx < bind_data.names.size(); col_idx++) {
				// For FRONTMATTER mode, data column names don't have prefix
				string col_name;
				if (bind_data.options.multi_document_mode == MultiDocumentMode::FRONTMATTER) {
					col_name = bind_data.names[col_idx]; // Already the right name
				} else {
					col_name = bind_data.names[col_idx];
				}

				LogicalType &type = bind_data.types[col_idx];

				// Get the value for this column from the data node
				YAML::Node value = node[col_name];

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

//===--------------------------------------------------------------------===//
// parse_yaml Table Function - Parse YAML strings into rows
//===--------------------------------------------------------------------===//

// Bind data structure for parse_yaml (immutable after bind)
struct ParseYAMLBindData : public TableFunctionData {
	ParseYAMLBindData() = default;

	vector<YAML::Node> yaml_docs;                                    // Parsed YAML documents
	vector<string> names;                                            // Column names
	vector<LogicalType> types;                                       // Column types
	MultiDocumentMode multi_document_mode = MultiDocumentMode::ROWS; // How to handle multi-document YAML
	bool expand_root_sequence = true;                                // Whether to expand top-level sequences
	bool frontmatter_as_columns = true;                              // For FRONTMATTER mode
	string list_column_name = "documents";                           // For LIST mode
};

// Local state for parse_yaml (mutable execution state)
struct ParseYAMLLocalState : public LocalTableFunctionState {
	idx_t current_row = 0;
};

unique_ptr<FunctionData> YAMLReader::ParseYAMLBind(ClientContext &context, TableFunctionBindInput &input,
                                                   vector<LogicalType> &return_types, vector<string> &names) {
	if (input.inputs.empty()) {
		throw BinderException("parse_yaml requires a YAML string parameter");
	}

	// Get the YAML string
	Value yaml_value = input.inputs[0];
	if (yaml_value.IsNull()) {
		throw BinderException("parse_yaml input cannot be NULL");
	}

	string yaml_str = yaml_value.ToString();

	auto result = make_uniq<ParseYAMLBindData>();

	// Parse optional parameters
	for (auto &param : input.named_parameters) {
		if (param.first == "multi_document") {
			result->multi_document_mode = ParseMultiDocumentMode(param.second);
		} else if (param.first == "expand_root_sequence") {
			result->expand_root_sequence = param.second.GetValue<bool>();
		} else if (param.first == "frontmatter_as_columns") {
			result->frontmatter_as_columns = param.second.GetValue<bool>();
		} else if (param.first == "list_column_name") {
			result->list_column_name = param.second.GetValue<string>();
			if (result->list_column_name.empty()) {
				throw BinderException("list_column_name cannot be empty");
			}
		}
	}

	// Parse the YAML string
	try {
		vector<YAML::Node> docs;
		if (result->multi_document_mode != MultiDocumentMode::FIRST) {
			std::stringstream ss(yaml_str);
			docs = YAML::LoadAll(ss);
		} else {
			docs.push_back(YAML::Load(yaml_str));
		}

		// Extract row nodes (expand sequences if needed)
		result->yaml_docs = ExtractRowNodes(docs, result->expand_root_sequence);
	} catch (const YAML::Exception &e) {
		throw InvalidInputException("Failed to parse YAML: %s", e.what());
	}

	if (result->yaml_docs.empty()) {
		// Empty result - return a single yaml column
		names.emplace_back("yaml");
		return_types.emplace_back(LogicalType::VARCHAR);
		result->names = names;
		result->types = return_types;
		return std::move(result);
	}

	// Detect schema from all documents using jagged schema detection
	LogicalType merged_type = DetectJaggedYAMLType(result->yaml_docs);

	if (merged_type.id() == LogicalTypeId::STRUCT) {
		// Struct type - use struct fields as columns
		auto &children = StructType::GetChildTypes(merged_type);
		for (auto &child : children) {
			names.push_back(child.first);
			return_types.push_back(child.second);
		}
	} else {
		// Non-struct type - return single yaml column
		names.emplace_back("yaml");
		return_types.emplace_back(merged_type);
	}

	result->names = names;
	result->types = return_types;
	return std::move(result);
}

unique_ptr<LocalTableFunctionState> YAMLReader::ParseYAMLInit(ExecutionContext &context, TableFunctionInitInput &input,
                                                              GlobalTableFunctionState *global_state) {
	return make_uniq<ParseYAMLLocalState>();
}

void YAMLReader::ParseYAMLFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->Cast<ParseYAMLBindData>();
	auto &local_state = data_p.local_state->Cast<ParseYAMLLocalState>();

	// If we've processed all rows, we're done
	if (local_state.current_row >= bind_data.yaml_docs.size()) {
		output.SetCardinality(0);
		return;
	}

	// Process up to STANDARD_VECTOR_SIZE rows at a time
	idx_t count = 0;
	idx_t max_count = std::min((idx_t)STANDARD_VECTOR_SIZE, bind_data.yaml_docs.size() - local_state.current_row);

	output.Reset();

	for (idx_t doc_idx = 0; doc_idx < max_count; doc_idx++) {
		YAML::Node node = bind_data.yaml_docs[local_state.current_row + doc_idx];

		if (node.IsMap()) {
			// Map node - process each field as a column
			for (idx_t col_idx = 0; col_idx < bind_data.names.size(); col_idx++) {
				const string &col_name = bind_data.names[col_idx];
				const LogicalType &col_type = bind_data.types[col_idx];

				if (node[col_name]) {
					Value val = YAMLNodeToValue(node[col_name], col_type);
					output.SetValue(col_idx, count, val);
				} else {
					output.SetValue(col_idx, count, Value(col_type));
				}
			}
		} else if (bind_data.types.size() == 1) {
			// Non-map node with single column output
			Value val = YAMLNodeToValue(node, bind_data.types[0]);
			output.SetValue(0, count, val);
		}
		count++;
	}

	local_state.current_row += count;
	output.SetCardinality(count);
}

} // namespace duckdb
