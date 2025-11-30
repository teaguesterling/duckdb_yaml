#include "yaml_reader.hpp"
#include "yaml_extension.hpp"
#include "duckdb/catalog/catalog_entry/table_function_catalog_entry.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include <unordered_set>

namespace duckdb {

// Options for read_yaml_frontmatter
struct YAMLFrontmatterOptions {
	bool as_yaml_objects = false;  // If true, expand fields as columns; if false, return single YAML column
	bool include_content = false;  // If true, include file content after frontmatter
	bool include_filename = false; // If true, include filename column
};

// Bind data for read_yaml_frontmatter
struct YAMLFrontmatterBindData : public TableFunctionData {
	vector<string> file_paths;
	YAMLFrontmatterOptions options;
	vector<string> names;
	vector<LogicalType> types;
};

// Local state for read_yaml_frontmatter
struct YAMLFrontmatterLocalState : public LocalTableFunctionState {
	idx_t current_file = 0;
};

// Extract frontmatter from file content
// Returns a pair of (frontmatter_yaml, body_content)
// If no frontmatter found, returns empty frontmatter
static pair<string, string> ExtractFrontmatter(const string &content) {
	// Frontmatter must start with "---" at the beginning of the file
	if (content.size() < 3 || content.substr(0, 3) != "---") {
		return {"", content};
	}

	// Find the end delimiter (--- or ...)
	size_t start = 3;
	// Skip whitespace/newline after opening ---
	while (start < content.size() && (content[start] == ' ' || content[start] == '\t')) {
		start++;
	}
	if (start < content.size() && content[start] == '\n') {
		start++;
	} else if (start + 1 < content.size() && content[start] == '\r' && content[start + 1] == '\n') {
		start += 2;
	}

	// Find closing delimiter
	size_t end_pos = string::npos;
	size_t search_pos = start;

	while (search_pos < content.size()) {
		// Look for \n--- or \n...
		size_t newline_pos = content.find('\n', search_pos);
		if (newline_pos == string::npos) {
			break;
		}

		size_t line_start = newline_pos + 1;
		if (line_start + 3 <= content.size()) {
			string potential_delim = content.substr(line_start, 3);
			if (potential_delim == "---" || potential_delim == "...") {
				// Check that it's followed by newline or end of file or whitespace
				if (line_start + 3 >= content.size() ||
				    content[line_start + 3] == '\n' ||
				    content[line_start + 3] == '\r' ||
				    content[line_start + 3] == ' ' ||
				    content[line_start + 3] == '\t') {
					end_pos = newline_pos;
					break;
				}
			}
		}
		search_pos = newline_pos + 1;
	}

	if (end_pos == string::npos) {
		// No closing delimiter found - treat entire content as body
		return {"", content};
	}

	string frontmatter = content.substr(start, end_pos - start);

	// Find start of body (after closing delimiter line)
	size_t body_start = end_pos + 1; // skip the \n before ---
	body_start += 3; // skip ---
	// Skip rest of delimiter line
	while (body_start < content.size() && content[body_start] != '\n' && content[body_start] != '\r') {
		body_start++;
	}
	// Skip the newline
	if (body_start < content.size() && content[body_start] == '\r') {
		body_start++;
	}
	if (body_start < content.size() && content[body_start] == '\n') {
		body_start++;
	}

	string body = body_start < content.size() ? content.substr(body_start) : "";

	return {frontmatter, body};
}

// Read file content
static string ReadFileContent(ClientContext &context, const string &file_path) {
	auto &fs = FileSystem::GetFileSystem(context);
	auto handle = fs.OpenFile(file_path, FileFlags::FILE_FLAGS_READ);
	auto file_size = handle->GetFileSize();

	string content;
	content.resize(file_size);
	handle->Read((void *)content.data(), file_size);

	return content;
}

// Bind function for read_yaml_frontmatter
static unique_ptr<FunctionData> YAMLFrontmatterBind(ClientContext &context, TableFunctionBindInput &input,
                                                     vector<LogicalType> &return_types, vector<string> &names) {
	auto result = make_uniq<YAMLFrontmatterBindData>();

	// Get file paths from first argument
	if (input.inputs.empty()) {
		throw BinderException("read_yaml_frontmatter requires a file path parameter");
	}

	result->file_paths = YAMLReader::GetFiles(context, input.inputs[0], false);

	if (result->file_paths.empty()) {
		throw BinderException("No files found matching the provided path");
	}

	// Parse named parameters
	for (auto &kv : input.named_parameters) {
		if (kv.first == "as_yaml_objects") {
			result->options.as_yaml_objects = BooleanValue::Get(kv.second);
		} else if (kv.first == "content") {
			result->options.include_content = BooleanValue::Get(kv.second);
		} else if (kv.first == "filename") {
			result->options.include_filename = BooleanValue::Get(kv.second);
		}
	}

	// Build schema based on options
	if (result->options.include_filename) {
		names.push_back("filename");
		return_types.push_back(LogicalType::VARCHAR);
	}

	if (!result->options.as_yaml_objects) {
		// Default: expand fields as columns by merging schemas from all files
		unordered_map<string, LogicalType> merged_types;
		vector<string> column_order;
		unordered_set<string> seen_columns;

		for (const auto &file_path : result->file_paths) {
			try {
				string content = ReadFileContent(context, file_path);
				auto extracted = ExtractFrontmatter(content);
				string frontmatter = extracted.first;

				if (frontmatter.empty()) {
					continue;
				}

				// Parse the frontmatter YAML
				YAML::Node node = YAML::Load(frontmatter);

				if (!node.IsMap()) {
					continue;
				}

				// Process each field
				for (auto it = node.begin(); it != node.end(); ++it) {
					string key = it->first.Scalar();

					// Track column order from first occurrence
					if (seen_columns.find(key) == seen_columns.end()) {
						column_order.push_back(key);
						seen_columns.insert(key);
					}

					// Detect type
					LogicalType value_type = YAMLReader::DetectYAMLType(it->second);

					// Merge with existing type
					auto existing = merged_types.find(key);
					if (existing == merged_types.end()) {
						merged_types[key] = value_type;
					} else if (existing->second.id() == LogicalTypeId::STRUCT &&
					           value_type.id() == LogicalTypeId::STRUCT) {
						merged_types[key] = YAMLReader::MergeStructTypes(existing->second, value_type);
					} else if (existing->second.id() != value_type.id()) {
						merged_types[key] = LogicalType::VARCHAR;
					}
				}
			} catch (...) {
				// Skip files that fail to parse
				continue;
			}
		}

		// Add columns in order
		for (const auto &col : column_order) {
			names.push_back(col);
			return_types.push_back(merged_types[col]);
		}

		// If no fields detected, add a dummy column
		if (names.empty() || (result->options.include_filename && names.size() == 1)) {
			names.push_back("frontmatter");
			return_types.push_back(LogicalType::VARCHAR);
		}
	} else {
		// Single frontmatter column as YAML type
		names.push_back("frontmatter");
		// Use VARCHAR with YAML alias
		LogicalType yaml_type = LogicalType::VARCHAR;
		yaml_type.SetAlias("YAML");
		return_types.push_back(yaml_type);
	}

	if (result->options.include_content) {
		names.push_back("content");
		return_types.push_back(LogicalType::VARCHAR);
	}

	// Store schema
	result->names = names;
	result->types = return_types;

	return std::move(result);
}

// Init local state function
static unique_ptr<LocalTableFunctionState> YAMLFrontmatterInit(ExecutionContext &context,
                                                                 TableFunctionInitInput &input,
                                                                 GlobalTableFunctionState *global_state) {
	return make_uniq<YAMLFrontmatterLocalState>();
}

// Execution function for read_yaml_frontmatter
static void YAMLFrontmatterFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->Cast<YAMLFrontmatterBindData>();
	auto &local_state = data_p.local_state->Cast<YAMLFrontmatterLocalState>();

	if (local_state.current_file >= bind_data.file_paths.size()) {
		output.SetCardinality(0);
		return;
	}

	output.Reset();
	idx_t count = 0;
	idx_t max_count = STANDARD_VECTOR_SIZE;

	while (count < max_count && local_state.current_file < bind_data.file_paths.size()) {
		const string &file_path = bind_data.file_paths[local_state.current_file];
		local_state.current_file++;

		try {
			string content = ReadFileContent(context, file_path);
			auto extracted = ExtractFrontmatter(content);
			string frontmatter = extracted.first;
			string body = extracted.second;

			idx_t col_idx = 0;

			// Filename column
			if (bind_data.options.include_filename) {
				output.SetValue(col_idx++, count, Value(file_path));
			}

			if (!bind_data.options.as_yaml_objects) {
				// Default: parse frontmatter and extract fields as columns
				if (!frontmatter.empty()) {
					try {
						YAML::Node node = YAML::Load(frontmatter);

						if (node.IsMap()) {
							// Process each column (skip filename if present)
							idx_t start_col = bind_data.options.include_filename ? 1 : 0;
							idx_t end_col = bind_data.options.include_content ?
							                bind_data.names.size() - 1 : bind_data.names.size();

							for (idx_t i = start_col; i < end_col; i++) {
								const string &col_name = bind_data.names[i];
								YAML::Node value = node[col_name];

								if (value) {
									output.SetValue(col_idx, count,
									                YAMLReader::YAMLNodeToValue(value, bind_data.types[i]));
								} else {
									output.SetValue(col_idx, count, Value(bind_data.types[i]));
								}
								col_idx++;
							}
						} else {
							// Non-map frontmatter - set all fields to NULL
							idx_t start_col = bind_data.options.include_filename ? 1 : 0;
							idx_t end_col = bind_data.options.include_content ?
							                bind_data.names.size() - 1 : bind_data.names.size();
							for (idx_t i = start_col; i < end_col; i++) {
								output.SetValue(col_idx++, count, Value(bind_data.types[i]));
							}
						}
					} catch (...) {
						// Parse error - set all fields to NULL
						idx_t start_col = bind_data.options.include_filename ? 1 : 0;
						idx_t end_col = bind_data.options.include_content ?
						                bind_data.names.size() - 1 : bind_data.names.size();
						for (idx_t i = start_col; i < end_col; i++) {
							output.SetValue(col_idx++, count, Value(bind_data.types[i]));
						}
					}
				} else {
					// No frontmatter - set all fields to NULL
					idx_t start_col = bind_data.options.include_filename ? 1 : 0;
					idx_t end_col = bind_data.options.include_content ?
					                bind_data.names.size() - 1 : bind_data.names.size();
					for (idx_t i = start_col; i < end_col; i++) {
						output.SetValue(col_idx++, count, Value(bind_data.types[i]));
					}
				}
			} else {
				// Return frontmatter as YAML string
				if (!frontmatter.empty()) {
					output.SetValue(col_idx++, count, Value(frontmatter));
				} else {
					output.SetValue(col_idx++, count, Value(bind_data.types[col_idx - 1]));
				}
			}

			// Content column
			if (bind_data.options.include_content) {
				output.SetValue(col_idx++, count, Value(body));
			}

			count++;
		} catch (const std::exception &e) {
			// Skip files that can't be read
			continue;
		}
	}

	output.SetCardinality(count);
}

void RegisterYAMLFrontmatterFunction(ExtensionLoader &loader) {
	TableFunction read_yaml_frontmatter("read_yaml_frontmatter", {LogicalType::ANY},
	                                     YAMLFrontmatterFunction, YAMLFrontmatterBind);

	// Set init function for local state
	read_yaml_frontmatter.init_local = YAMLFrontmatterInit;

	// Add named parameters
	read_yaml_frontmatter.named_parameters["as_yaml_objects"] = LogicalType::BOOLEAN;
	read_yaml_frontmatter.named_parameters["content"] = LogicalType::BOOLEAN;
	read_yaml_frontmatter.named_parameters["filename"] = LogicalType::BOOLEAN;

	loader.RegisterFunction(read_yaml_frontmatter);
}

} // namespace duckdb
