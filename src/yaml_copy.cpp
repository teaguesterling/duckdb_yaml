#include "duckdb/function/copy_function.hpp"
#include "duckdb_compat.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/mutex.hpp"
#include "duckdb/common/serializer/buffered_file_writer.hpp"
#include "duckdb/common/types/column/column_data_collection.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/expression/positional_reference_expression.hpp"
#include "duckdb/parser/query_node/select_node.hpp"
#include "duckdb/parser/tableref/subqueryref.hpp"
#include "duckdb/planner/binder.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "yaml_types.hpp"
#include "yaml_utils.hpp"
#include "yaml_formatting.hpp"

namespace duckdb {

static void ThrowYAMLCopyParameterException(const string &loption) {
	throw BinderException("COPY (FORMAT YAML) parameter %s expects a single argument.", loption);
}

static BoundStatement CopyToYAMLPlan(Binder &binder, CopyStatement &stmt) {
	auto stmt_copy = stmt.Copy();
	auto &copy = stmt_copy->Cast<CopyStatement>();
	auto &copied_info = *copy.info;

	// Parse YAML-specific options, creating options for the yaml_rows writer
	string yaml_style = "";     // Default to empty, will default to "block"
	string yaml_layout = "";    // Default to empty, will infer from style
	string yaml_multiline = ""; // Default to empty, will resolve to "auto"
	string yaml_indent = "";    // Default to empty, will use "2"
	bool explicit_start = false;
	// Typed off CopyInfo::options rather than spelled out: DuckDB v2.0 rekeyed
	// that map from case_insensitive_map_t<...> (string keys) to
	// identifier_map_t<...> (Identifier keys), and the two do not assign to each
	// other. Deriving the local's type from the member it is moved into keeps the
	// assignment at the bottom of this function correct on both lines, and keeps
	// working if the map changes again. The literal keys need no helper --
	// Identifier(const char *) is implicit by design.
	decltype(copied_info.options) writer_options {{"file_extension", {"yaml"}}};

	for (const auto &kv : copied_info.options) {
		// COPY option keys are identifiers on v2.0, so read the raw name out.
		const auto loption = StringUtil::Lower(CompatIdentifierName(kv.first));
		if (loption == "style") {
			if (kv.second.size() != 1) {
				ThrowYAMLCopyParameterException(loption);
			}
			yaml_style = StringValue::Get(kv.second.back());
			auto lowercase_style = StringUtil::Lower(yaml_style);
			if (lowercase_style != "block" && lowercase_style != "flow") {
				throw BinderException("Invalid YAML style '%s'. Valid options are 'flow' or 'block'.",
				                      yaml_style.c_str());
			}
		} else if (loption == "layout") {
			if (kv.second.size() != 1) {
				ThrowYAMLCopyParameterException(loption);
			}
			yaml_layout = StringValue::Get(kv.second.back());
			auto lowercase_layout = StringUtil::Lower(yaml_layout);
			if (lowercase_layout != "sequence" && lowercase_layout != "document") {
				throw BinderException("Invalid YAML layout '%s'. Valid options are 'sequence' or 'document'.",
				                      yaml_layout.c_str());
			}
		} else if (loption == "multiline") {
			if (kv.second.size() != 1) {
				ThrowYAMLCopyParameterException(loption);
			}
			yaml_multiline = StringValue::Get(kv.second.back());
			auto lowercase_multiline = StringUtil::Lower(yaml_multiline);
			if (lowercase_multiline != "auto" && lowercase_multiline != "literal" && lowercase_multiline != "quoted") {
				throw BinderException("Invalid YAML multiline '%s'. Valid options are 'auto', 'literal', or 'quoted'.",
				                      yaml_multiline.c_str());
			}
		} else if (loption == "indent") {
			if (kv.second.size() != 1) {
				ThrowYAMLCopyParameterException(loption);
			}
			yaml_indent = kv.second.back().ToString();
			int indent_val;
			try {
				indent_val = std::stoi(yaml_indent);
			} catch (...) {
				throw BinderException("Invalid YAML indent '%s'. Must be an integer between 1 and 10.",
				                      yaml_indent.c_str());
			}
			if (indent_val < 1 || indent_val > 10) {
				throw BinderException("Invalid YAML indent '%s'. Must be an integer between 1 and 10.",
				                      yaml_indent.c_str());
			}
		} else if (loption == "explicit_start") {
			if (kv.second.size() > 1) {
				ThrowYAMLCopyParameterException(loption);
			}
			explicit_start = kv.second.empty() || kv.second.back().DefaultCastAs(LogicalType::BOOLEAN).GetValue<bool>();
		} else if (loption == "encoding") {
			throw InvalidInputException("Option \"ENCODING\" is not supported for writing - only for reading");
		} else if (loption == "compression" || loption == "per_thread_output" || loption == "file_size_bytes" ||
		           loption == "use_tmp_file" || loption == "overwrite_or_ignore" || loption == "filename_pattern" ||
		           loption == "file_extension") {
			// We support these base options
			writer_options.insert(kv);
		} else {
			throw BinderException("Unknown option for COPY ... TO ... (FORMAT YAML): \"%s\".", loption);
		}
	}

	// Only block-style documents have a separator. A sequence item or a flow line needs none.
	const bool separated = StringUtil::Lower(yaml_layout) != "sequence" && StringUtil::Lower(yaml_style) != "flow";
	if (explicit_start && !separated) {
		throw BinderException("EXPLICIT_START requires LAYOUT document and STYLE block: other output has no `---`.");
	}

	// Bind the select statement of the original to resolve the types
	auto dummy_binder = Binder::CreateBinder(binder.context, &binder);
	auto bound_original = dummy_binder->Bind(*stmt.info->select_statement);

	// Create new SelectNode with the original SelectNode as a subquery in the FROM clause
	auto select_stmt = make_uniq<SelectStatement>();
	select_stmt->node = std::move(copied_info.select_statement);
	auto subquery_ref = make_uniq<SubqueryRef>(std::move(select_stmt));

	copied_info.select_statement = make_uniq_base<QueryNode, SelectNode>();
	auto &select_node = copied_info.select_statement->Cast<SelectNode>();
	select_node.from_table = std::move(subquery_ref);

	// Create new select list - each column becomes a positional reference with alias
	vector<unique_ptr<ParsedExpression>> select_list;
	select_list.reserve(bound_original.types.size());

	for (idx_t col_idx = 0; col_idx < bound_original.types.size(); col_idx++) {
		const auto &name = bound_original.names[col_idx];

		// Create positional reference and set alias (needed for struct_pack named fields)
		auto column = make_uniq_base<ParsedExpression, PositionalReferenceExpression>(col_idx + 1);
		column->SetAlias(name);
		select_list.emplace_back(std::move(column));
	}

	// Now create the struct_pack to create a struct with all columns
	// struct_pack uses the aliases automatically as field names
	auto struct_pack_expr = make_uniq<FunctionExpression>("struct_pack", std::move(select_list));

	// Now convert to YAML using format_yaml
	vector<unique_ptr<ParsedExpression>> format_yaml_children;
	format_yaml_children.emplace_back(std::move(struct_pack_expr));

	// Add style parameter if specified (using named parameter syntax)
	if (!yaml_style.empty()) {
		// Create named parameter: style := 'block' or style := 'flow'
		auto style_value = CompatConstant(Value(yaml_style));
		style_value->SetAlias("style"); // This creates the named parameter
		format_yaml_children.emplace_back(std::move(style_value));
	}

	// For COPY TO, always use layout=document internally to avoid array wrapping
	// The actual layout transformation will be handled via post-processing
	auto internal_layout_value = CompatConstant(Value("document"));
	internal_layout_value->SetAlias("layout");
	format_yaml_children.emplace_back(std::move(internal_layout_value));

	// Add the target layout, style, multiline, and indent as trailing positional args
	if (!yaml_layout.empty()) {
		auto target_layout_value = CompatConstant(Value(yaml_layout));
		format_yaml_children.emplace_back(std::move(target_layout_value));
	} else {
		auto target_layout_value = CompatConstant(Value("document")); // Default
		format_yaml_children.emplace_back(std::move(target_layout_value));
	}

	if (!yaml_style.empty()) {
		auto target_style_value = CompatConstant(Value(yaml_style));
		format_yaml_children.emplace_back(std::move(target_style_value));
	} else {
		auto target_style_value = CompatConstant(Value("block")); // Default changed to block
		format_yaml_children.emplace_back(std::move(target_style_value));
	}

	// Multiline style (auto/literal/quoted)
	if (!yaml_multiline.empty()) {
		auto multiline_value = CompatConstant(Value(yaml_multiline));
		format_yaml_children.emplace_back(std::move(multiline_value));
	} else {
		auto multiline_value = CompatConstant(Value("auto")); // Default
		format_yaml_children.emplace_back(std::move(multiline_value));
	}

	// Indent size
	if (!yaml_indent.empty()) {
		auto indent_value = CompatConstant(Value(yaml_indent));
		format_yaml_children.emplace_back(std::move(indent_value));
	} else {
		auto indent_value = CompatConstant(Value("2")); // Default
		format_yaml_children.emplace_back(std::move(indent_value));
	}

	// Create copy_format_yaml function call (we'll register this function to handle post-processing)
	select_node.select_list.emplace_back(
	    make_uniq<FunctionExpression>("copy_format_yaml", std::move(format_yaml_children)));

	// Formatting stays in the projection, so it runs in parallel. yaml_rows writes the formatted rows to the file.
	copied_info.format = "yaml_rows";
	copied_info.options = std::move(writer_options);
	copied_info.options["separated"] = {Value::BOOLEAN(separated)};
	copied_info.options["explicit_start"] = {Value::BOOLEAN(explicit_start)};

	return binder.Bind(*stmt_copy);
}

//===--------------------------------------------------------------------===//
// COPY Format YAML Function
//===--------------------------------------------------------------------===//

static void CopyFormatYAMLFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &input = args.data[0];

	// Get trailing positional args: layout, style, multiline, indent (last 4)
	auto col_count = args.ColumnCount();
	Value target_layout_value = args.data[col_count - 4].GetValue(0);
	Value target_style_value = args.data[col_count - 3].GetValue(0);
	Value target_multiline_value = args.data[col_count - 2].GetValue(0);
	Value target_indent_value = args.data[col_count - 1].GetValue(0);

	string target_layout_str = StringUtil::Lower(target_layout_value.ToString());
	string target_style = StringUtil::Lower(target_style_value.ToString());
	string target_multiline = StringUtil::Lower(target_multiline_value.ToString());
	idx_t indent = static_cast<idx_t>(std::stoi(target_indent_value.ToString()));

	// Convert layout string to enum
	yaml_formatting::YAMLLayout layout = yaml_formatting::YAMLLayout::DOCUMENT;
	if (target_layout_str == "sequence") {
		layout = yaml_formatting::YAMLLayout::SEQUENCE;
	}

	// Determine YAML format from target style
	yaml_utils::YAMLFormat format = yaml_utils::YAMLSettings::GetDefaultFormat();
	if (target_style == "block") {
		format = yaml_utils::YAMLFormat::BLOCK;
	} else if (target_style == "flow") {
		format = yaml_utils::YAMLFormat::FLOW;
	}

	// Determine multiline string style
	yaml_utils::YAMLStringStyle string_style = yaml_utils::YAMLStringStyle::AUTO;
	if (target_multiline == "literal") {
		string_style = yaml_utils::YAMLStringStyle::LITERAL;
	} else if (target_multiline == "quoted") {
		string_style = yaml_utils::YAMLStringStyle::QUOTED;
	}

	// Process each row with post-processing
	for (idx_t row_idx = 0; row_idx < args.size(); row_idx++) {
		Value value = input.GetValue(row_idx);

		try {
			// Get the base YAML string with string style and indent
			std::string yaml_str = yaml_utils::ValueToYAMLString(value, format, string_style, indent);

			// Apply layout-specific formatting. The writer adds document separators, because only it knows the file.
			yaml_str = yaml_formatting::PostProcessForLayout(yaml_str, layout);

			result.SetValue(row_idx, Value(yaml_str));
		} catch (const std::exception &e) {
			result.SetValue(row_idx, Value("null"));
		} catch (...) {
			result.SetValue(row_idx, Value("null"));
		}
	}
}

//===--------------------------------------------------------------------===//
// yaml_rows: writes formatted rows to a file
//===--------------------------------------------------------------------===//
//
// The target of the rewrite in CopyToYAMLPlan. Its input is one VARCHAR column of formatted rows.
//
// A separator comes before every document except, by default, the first document of each FILE. Only this writer can
// know that: the formatting function sees one chunk at a time, in any order. A row with no separator anywhere but
// byte 0 merges into the previous document and is lost, so the rule has one implementation:
//
//   - A buffer holds rows with separators between them, and none before its first row.
//   - YAMLRowsGlobalState::Write adds the separator before a buffer, under a lock, from the state of that file.

static const char YAML_DOCUMENT_SEPARATOR[] = "---\n";
// The size that the CSV writer flushes at. Rotation sees only bytes that reached the file, so this also limits how far
// a file can go over FILE_SIZE_BYTES.
static constexpr idx_t YAML_ROWS_FLUSH_SIZE = 4096 * 8;

struct YAMLRowsBindData : public FunctionData {
	FileCompressionType compression = FileCompressionType::AUTO_DETECT;
	bool separated = false;
	bool explicit_start = false;

	unique_ptr<FunctionData> Copy() const override {
		auto result = make_uniq<YAMLRowsBindData>();
		result->compression = compression;
		result->separated = separated;
		result->explicit_start = explicit_start;
		return std::move(result);
	}

	bool Equals(const FunctionData &other_p) const override {
		auto &other = other_p.Cast<YAMLRowsBindData>();
		return compression == other.compression && separated == other.separated &&
		       explicit_start == other.explicit_start;
	}
};

static FileOpenFlags YAMLRowsOpenFlags(ClientContext &context, const string &file_path,
                                       const FileCompressionType &compression) {
	auto &fs = FileSystem::GetFileSystem(context);
	auto flags = FileFlags::FILE_FLAGS_WRITE | FileFlags::FILE_FLAGS_FILE_CREATE_NEW | compression;
	if (!fs.FileExists(file_path) && !fs.IsPipe(file_path)) {
		flags |= FileFlags::FILE_FLAGS_EXCLUSIVE_CREATE;
	}
	return flags;
}

struct YAMLRowsGlobalState : public GlobalFunctionData {
	YAMLRowsGlobalState(ClientContext &context, const string &file_path, const YAMLRowsBindData &bind_data)
	    : writer(FileSystem::GetFileSystem(context), file_path,
	             YAMLRowsOpenFlags(context, file_path, bind_data.compression)),
	      write_separator(bind_data.separated && bind_data.explicit_start), separated(bind_data.separated) {
	}

	void Write(const string &rows) {
		if (rows.empty()) {
			return;
		}
		lock_guard<mutex> guard(lock);
		if (write_separator) {
			writer.WriteData(const_data_ptr_cast(YAML_DOCUMENT_SEPARATOR), sizeof(YAML_DOCUMENT_SEPARATOR) - 1);
		}
		writer.WriteData(const_data_ptr_cast(rows.data()), rows.size());
		// Every later buffer in this file follows a document
		write_separator = separated;
	}

	idx_t FileSize() {
		lock_guard<mutex> guard(lock);
		return writer.GetFileSize();
	}

	void Close() {
		lock_guard<mutex> guard(lock);
		writer.Close();
	}

private:
	mutex lock;
	BufferedFileWriter writer;
	bool write_separator;
	const bool separated;
};

struct YAMLRowsLocalState : public LocalFunctionData {
	string rows;
};

struct YAMLRowsBatch : public PreparedBatchData {
	string rows;
};

static void AppendYAMLRows(DataChunk &chunk, const YAMLRowsBindData &bind_data, string &rows) {
	auto &input = chunk.data[0];
	for (idx_t row_idx = 0; row_idx < chunk.size(); row_idx++) {
		if (bind_data.separated && !rows.empty()) {
			rows += YAML_DOCUMENT_SEPARATOR;
		}
		const auto row = input.GetValue(row_idx);
		if (row.IsNull()) {
			throw InvalidInputException("COPY (FORMAT yaml_rows) cannot write a NULL row");
		}
		rows += StringValue::Get(row);
		rows += '\n';
	}
}

static unique_ptr<FunctionData> YAMLRowsBind(ClientContext &context, CopyFunctionBindInput &input,
                                             const vector<CompatName> &names, const vector<LogicalType> &sql_types) {
	// Reachable directly as FORMAT yaml_rows, so the rewrite in CopyToYAMLPlan cannot be assumed
	if (sql_types.size() != 1 || sql_types[0].id() != LogicalTypeId::VARCHAR) {
		throw BinderException("COPY (FORMAT yaml_rows) requires one VARCHAR column of formatted rows.");
	}

	auto result = make_uniq<YAMLRowsBindData>();
	for (const auto &kv : input.info.options) {
		const auto loption = StringUtil::Lower(CompatIdentifierName(kv.first));
		if (loption == "separated") {
			result->separated = BooleanValue::Get(kv.second.back());
		} else if (loption == "explicit_start") {
			result->explicit_start = BooleanValue::Get(kv.second.back());
		} else if (loption == "compression") {
			if (kv.second.size() != 1) {
				ThrowYAMLCopyParameterException(loption);
			}
			const auto name = StringUtil::Lower(kv.second.back().ToString());
			if (name == "auto" || name == "infer") {
				result->compression = FileCompressionType::AUTO_DETECT;
			} else if (name == "gzip") {
				result->compression = FileCompressionType::GZIP;
			} else if (name == "zstd") {
				result->compression = FileCompressionType::ZSTD;
			} else if (name == "uncompressed" || name == "none" || name.empty()) {
				result->compression = FileCompressionType::UNCOMPRESSED;
			} else {
				throw ParserException("Unrecognized file compression type \"%s\"", name);
			}
		} else {
			throw BinderException("Unknown option for COPY ... TO ... (FORMAT YAML): \"%s\".", loption);
		}
	}
	// Generated file names take the extension of the compression, as with the other COPY formats
	string suffix;
	if (result->compression == FileCompressionType::GZIP) {
		suffix = ".gz";
	} else if (result->compression == FileCompressionType::ZSTD) {
		suffix = ".zst";
	}
	if (!suffix.empty() && !StringUtil::EndsWith(input.file_extension, suffix)) {
		input.file_extension += suffix;
	}
	return std::move(result);
}

static void YAMLRowsOptions(ClientContext &context, CopyOptionsInput &input) {
	input.options["compression"] = CopyOption(LogicalType::VARCHAR, CopyOptionMode::WRITE_ONLY);
	input.options["separated"] = CopyOption(LogicalType::BOOLEAN, CopyOptionMode::WRITE_ONLY);
	input.options["explicit_start"] = CopyOption(LogicalType::BOOLEAN, CopyOptionMode::WRITE_ONLY);
}

static unique_ptr<LocalFunctionData> YAMLRowsInitializeLocal(ExecutionContext &context, FunctionData &bind_data) {
	return make_uniq<YAMLRowsLocalState>();
}

static unique_ptr<GlobalFunctionData> YAMLRowsInitializeGlobal(ClientContext &context, FunctionData &bind_data,
                                                               const string &file_path) {
	return make_uniq<YAMLRowsGlobalState>(context, file_path, bind_data.Cast<YAMLRowsBindData>());
}

static void YAMLRowsSink(ExecutionContext &context, FunctionData &bind_data, GlobalFunctionData &gstate,
                         LocalFunctionData &lstate, DataChunk &input) {
	auto &local_state = lstate.Cast<YAMLRowsLocalState>();
	AppendYAMLRows(input, bind_data.Cast<YAMLRowsBindData>(), local_state.rows);
	if (local_state.rows.size() >= YAML_ROWS_FLUSH_SIZE) {
		gstate.Cast<YAMLRowsGlobalState>().Write(local_state.rows);
		local_state.rows.clear();
	}
}

static void YAMLRowsCombine(ExecutionContext &context, FunctionData &bind_data, GlobalFunctionData &gstate,
                            LocalFunctionData &lstate) {
	auto &local_state = lstate.Cast<YAMLRowsLocalState>();
	gstate.Cast<YAMLRowsGlobalState>().Write(local_state.rows);
	local_state.rows.clear();
}

static void YAMLRowsFinalize(ClientContext &context, FunctionData &bind_data, GlobalFunctionData &gstate) {
	gstate.Cast<YAMLRowsGlobalState>().Close();
}

static CopyFunctionExecutionMode YAMLRowsExecutionMode(bool preserve_insertion_order, bool supports_batch_index) {
	if (!preserve_insertion_order) {
		return CopyFunctionExecutionMode::PARALLEL_COPY_TO_FILE;
	}
	if (supports_batch_index) {
		return CopyFunctionExecutionMode::BATCH_COPY_TO_FILE;
	}
	return CopyFunctionExecutionMode::REGULAR_COPY_TO_FILE;
}

static unique_ptr<PreparedBatchData> YAMLRowsPrepareBatch(ClientContext &context, FunctionData &bind_data,
                                                          GlobalFunctionData &gstate,
                                                          unique_ptr<ColumnDataCollection> collection) {
	auto batch = make_uniq<YAMLRowsBatch>();
	auto &yaml_bind_data = bind_data.Cast<YAMLRowsBindData>();
	for (auto &chunk : collection->Chunks()) {
		AppendYAMLRows(chunk, yaml_bind_data, batch->rows);
	}
	return std::move(batch);
}

static void YAMLRowsFlushBatch(ClientContext &context, FunctionData &bind_data, GlobalFunctionData &gstate,
                               PreparedBatchData &batch) {
	gstate.Cast<YAMLRowsGlobalState>().Write(batch.Cast<YAMLRowsBatch>().rows);
}

static idx_t YAMLRowsFileSizeBytes(GlobalFunctionData &gstate) {
	return gstate.Cast<YAMLRowsGlobalState>().FileSize();
}

static CopyFunction GetYAMLRowsCopyFunction() {
	CopyFunction function("yaml_rows");
	function.extension = "yaml";
	function.copy_options = YAMLRowsOptions;
	function.copy_to_bind = YAMLRowsBind;
	function.copy_to_initialize_local = YAMLRowsInitializeLocal;
	function.copy_to_initialize_global = YAMLRowsInitializeGlobal;
	function.copy_to_sink = YAMLRowsSink;
	function.copy_to_combine = YAMLRowsCombine;
	function.copy_to_finalize = YAMLRowsFinalize;
	function.execution_mode = YAMLRowsExecutionMode;
	function.prepare_batch = YAMLRowsPrepareBatch;
	function.flush_batch = YAMLRowsFlushBatch;
	function.file_size_bytes = YAMLRowsFileSizeBytes;
	return function;
}

void RegisterYAMLCopyFunctions(ExtensionLoader &loader) {
	// Register the COPY TO function
	auto copy_function = CopyFunction("yaml");
	copy_function.extension = "yaml";
	copy_function.plan = CopyToYAMLPlan;
	loader.RegisterFunction(copy_function);
	loader.RegisterFunction(GetYAMLRowsCopyFunction());

	// Register copy_format_yaml function for COPY TO post-processing
	auto copy_format_yaml_fun =
	    ScalarFunction("copy_format_yaml", {LogicalType::ANY}, LogicalType::VARCHAR, CopyFormatYAMLFunction);
	CompatSetScalarNullHandling(copy_format_yaml_fun, FunctionNullHandling::SPECIAL_HANDLING);
	CompatSetScalarVarArgs(copy_format_yaml_fun, LogicalType::ANY); // Allow variable number of arguments
	// Fallible: this can raise a runtime error on malformed input. DuckDB v2.0
	// rethrows an execution error from an unmarked function as an INTERNAL error
	// ("the function must call SetFallible()"). No-op on the pinned v1.5.x.
	CompatSetFallible(copy_format_yaml_fun);
	loader.RegisterFunction(copy_format_yaml_fun);
}

CopyFunction GetYAMLCopyFunction() {
	CopyFunction function("yaml");
	function.extension = "yaml";

	function.plan = CopyToYAMLPlan;

	// Note: We don't implement copy_from_bind or copy_from_function here
	// because YAML reading is handled by read_yaml table functions

	return function;
}

} // namespace duckdb
