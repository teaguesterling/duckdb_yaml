#include "yaml_reader.hpp"
#include "yaml_utils.hpp"
#include "duckdb_compat.hpp"
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

// True if a standalone frontmatter delimiter line begins at `pos`: exactly "---" (or
// "..." when `allow_document_end`) followed by nothing but spaces/tabs to the end of the
// line or the end of the input.
//
// Both the opening and the closing delimiter go through this so they cannot disagree.
// They used to: the opening test was a bare `content.substr(0, 3) == "---"` prefix match,
// so a plain markdown file whose first line was a thematic break (`----`, `-----`) or any
// `---foo` was treated as opening a frontmatter block. Everything up to the next `---`
// line was then swallowed as "frontmatter" -- which parses to nothing -- and only the tail
// was returned as the body, truncating the document with no error (issue #42). The closing
// test, meanwhile, already required a full-line match, so `----` opened a block that could
// never be closed by another `----`.
static bool IsDelimiterLine(const string &content, size_t pos, bool allow_document_end) {
	if (pos + 3 > content.size()) {
		return false;
	}
	bool is_marker = content.compare(pos, 3, "---") == 0 || (allow_document_end && content.compare(pos, 3, "...") == 0);
	if (!is_marker) {
		return false;
	}
	for (size_t i = pos + 3; i < content.size(); i++) {
		const char c = content[i];
		if (c == '\n' || c == '\r') {
			return true;
		}
		if (c != ' ' && c != '\t') {
			return false;
		}
	}
	return true; // delimiter runs to the end of the input
}

// Extract frontmatter from file content
// Returns a pair of (frontmatter_yaml, body_content)
// If no frontmatter found, returns empty frontmatter
static pair<string, string> ExtractFrontmatter(const string &content) {
	// A UTF-8 BOM is common in Windows-authored markdown. Without skipping it the opening
	// delimiter never matches and the file silently yields no frontmatter at all, even
	// though it has some (issue #42). Every other frontmatter reader (Jekyll, gray-matter,
	// python-frontmatter) strips the BOM before looking for the delimiter.
	size_t origin = 0;
	if (content.size() >= 3 && content.compare(0, 3, "\xEF\xBB\xBF") == 0) {
		origin = 3;
	}
	auto without_frontmatter = [&]() -> pair<string, string> {
		return {"", origin == 0 ? content : content.substr(origin)};
	};

	// Frontmatter must open with a "---" line at the very beginning of the file. "..." is a
	// document *end* marker and never opens a block.
	if (!IsDelimiterLine(content, origin, /*allow_document_end=*/false)) {
		return without_frontmatter();
	}

	// Move past the opening delimiter line.
	size_t start = origin + 3;
	while (start < content.size() && (content[start] == ' ' || content[start] == '\t')) {
		start++;
	}
	if (start < content.size() && content[start] == '\r') {
		start++;
	}
	if (start < content.size() && content[start] == '\n') {
		start++;
	}

	// Find closing delimiter.
	//
	// Iterate over line starts rather than over newlines. The previous version only ever
	// examined the line *following* a newline found at or beyond `start`, so the first line
	// of the block was never tested. A block closed immediately (`---\n---\n`, i.e. empty
	// frontmatter) was therefore missed, and the scan ran on to match a `---` in the body --
	// silently promoting body text to frontmatter and hiding the real body (issue #42).
	//
	// A delimiter only counts at column 0: YAML document markers are defined that way, so a
	// `---` indented inside a block scalar is scalar content and correctly does not close.
	size_t delim_start = string::npos; // first char of the closing delimiter line
	size_t line_start = start;

	while (line_start <= content.size()) {
		if (IsDelimiterLine(content, line_start, /*allow_document_end=*/true)) {
			delim_start = line_start;
			break;
		}
		size_t newline_pos = content.find('\n', line_start);
		if (newline_pos == string::npos) {
			break;
		}
		line_start = newline_pos + 1;
	}

	if (delim_start == string::npos) {
		// No closing delimiter found - treat entire content as body
		return without_frontmatter();
	}

	// The frontmatter runs from `start` up to the line break preceding the delimiter line.
	// Trim that break explicitly rather than assuming a single '\n': under CRLF the byte
	// before the delimiter is '\n' and the one before that is '\r', which would otherwise
	// be left dangling on the last frontmatter line.
	// When the delimiter is the first line of the block the frontmatter is empty.
	size_t fm_end = delim_start;
	if (fm_end > start && content[fm_end - 1] == '\n') {
		fm_end--;
	}
	if (fm_end > start && content[fm_end - 1] == '\r') {
		fm_end--;
	}
	string frontmatter = fm_end > start ? content.substr(start, fm_end - start) : "";

	// Find start of body (after closing delimiter line)
	size_t body_start = delim_start + 3; // skip ---
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

	// Enforce the input-size cap here too — the frontmatter reader previously
	// read the whole file with no limit (GHSA-h5hw-g5m6-vmjj, finding #3).
	yaml_utils::CheckInputSize(file_size, "read_yaml_frontmatter");

	string content;
	content.resize(file_size);
	handle->Read((void *)content.data(), file_size);

	return content;
}

// Bind function for read_yaml_frontmatter
static unique_ptr<FunctionData> YAMLFrontmatterBind(ClientContext &context, TableFunctionBindInput &input,
                                                    vector<LogicalType> &return_types, vector<CompatName> &names) {
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
		auto kv_name = CompatIdentifierName(kv.first);
		if (kv_name == "as_yaml_objects") {
			result->options.as_yaml_objects = BooleanValue::Get(kv.second);
		} else if (kv_name == "content") {
			result->options.include_content = BooleanValue::Get(kv.second);
		} else if (kv_name == "filename") {
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
						// Widen compatible numerics across frontmatter docs; VARCHAR otherwise (issue #42).
						if (existing->second.IsNumeric() && value_type.IsNumeric()) {
							merged_types[key] = YAMLReader::WidenConflictingScalarTypes(existing->second, value_type);
						} else {
							merged_types[key] = LogicalType::VARCHAR;
						}
					}
				}
			} catch (...) {
				// Skip files that fail to parse
				continue;
			}
		}

		// Add columns in order
		for (const auto &col : column_order) {
			names.push_back(CompatMakeName(col));
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
		// Use VARCHAR with YAML alias.
		// CompatWithAlias, not SetAlias: v2.0 removed LogicalType::SetAlias.
		// NOTE: the alias here is uppercase "YAML" while YAMLTypes::YAMLType()
		// (yaml_types.cpp) uses lowercase "yaml", and every GetAlias() check in
		// this extension compares case-sensitively against "yaml". Preserved
		// verbatim by this port so the compat change does not move behaviour;
		// see the PR description for the casing discrepancy.
		return_types.push_back(CompatWithAlias(LogicalType::VARCHAR, "YAML"));
	}

	if (result->options.include_content) {
		names.push_back("content");
		return_types.push_back(LogicalType::VARCHAR);
	}

	// Store schema
	result->names = CompatNameStrings(names);
	result->types = return_types;

	return std::move(result);
}

// Init local state function
static unique_ptr<LocalTableFunctionState> YAMLFrontmatterInit(ExecutionContext &context, TableFunctionInitInput &input,
                                                               GlobalTableFunctionState *global_state) {
	return make_uniq<YAMLFrontmatterLocalState>();
}

// Execution function for read_yaml_frontmatter
static void YAMLFrontmatterFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->Cast<YAMLFrontmatterBindData>();
	auto &local_state = data_p.local_state->Cast<YAMLFrontmatterLocalState>();

	if (local_state.current_file >= bind_data.file_paths.size()) {
		CompatSetOutputCardinality(output, 0);
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

			// Skip files with no frontmatter
			if (frontmatter.empty()) {
				continue;
			}

			idx_t col_idx = 0;

			// Filename column
			if (bind_data.options.include_filename) {
				output.SetValue(col_idx++, count, Value(file_path));
			}

			if (!bind_data.options.as_yaml_objects) {
				// Default: parse frontmatter and extract fields as columns
				try {
					YAML::Node node = YAML::Load(frontmatter);

					if (node.IsMap()) {
						// Process each column (skip filename if present)
						idx_t start_col = bind_data.options.include_filename ? 1 : 0;
						idx_t end_col =
						    bind_data.options.include_content ? bind_data.names.size() - 1 : bind_data.names.size();

						for (idx_t i = start_col; i < end_col; i++) {
							const string &col_name = bind_data.names[i];
							YAML::Node value = node[col_name];

							if (value) {
								output.SetValue(col_idx, count, YAMLReader::YAMLNodeToValue(value, bind_data.types[i]));
							} else {
								output.SetValue(col_idx, count, Value(bind_data.types[i]));
							}
							col_idx++;
						}
					} else {
						// Non-map frontmatter - set all fields to NULL
						idx_t start_col = bind_data.options.include_filename ? 1 : 0;
						idx_t end_col =
						    bind_data.options.include_content ? bind_data.names.size() - 1 : bind_data.names.size();
						for (idx_t i = start_col; i < end_col; i++) {
							output.SetValue(col_idx++, count, Value(bind_data.types[i]));
						}
					}
				} catch (...) {
					// Parse error - set all fields to NULL
					idx_t start_col = bind_data.options.include_filename ? 1 : 0;
					idx_t end_col =
					    bind_data.options.include_content ? bind_data.names.size() - 1 : bind_data.names.size();
					for (idx_t i = start_col; i < end_col; i++) {
						output.SetValue(col_idx++, count, Value(bind_data.types[i]));
					}
				}
			} else {
				// Return frontmatter as YAML string
				output.SetValue(col_idx++, count, Value(frontmatter));
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

	CompatSetOutputCardinality(output, count);
}

void RegisterYAMLFrontmatterFunction(ExtensionLoader &loader) {
	TableFunction read_yaml_frontmatter("read_yaml_frontmatter", {LogicalType::ANY}, YAMLFrontmatterFunction,
	                                    YAMLFrontmatterBind);

	// Set init function for local state
	read_yaml_frontmatter.init_local = YAMLFrontmatterInit;

	// Add named parameters
	read_yaml_frontmatter.named_parameters["as_yaml_objects"] = LogicalType::BOOLEAN;
	read_yaml_frontmatter.named_parameters["content"] = LogicalType::BOOLEAN;
	read_yaml_frontmatter.named_parameters["filename"] = LogicalType::BOOLEAN;

	loader.RegisterFunction(read_yaml_frontmatter);
}

} // namespace duckdb
