#include "yaml_reader.hpp"
#include "duckdb_compat.hpp"
#include "duckdb/catalog/catalog_entry/table_function_catalog_entry.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"

namespace duckdb {

unique_ptr<TableRef> YAMLReader::ReadYAMLReplacement(ClientContext &context, ReplacementScanInput &input,
                                                     optional_ptr<ReplacementScanData> data) {
	auto table_name = ReplacementScan::GetFullPath(input);
	if (!ReplacementScan::CanReplace(table_name, {"yaml", "yml"})) {
		return nullptr;
	}

	auto table_function = make_uniq<TableFunctionRef>();
	vector<unique_ptr<ParsedExpression>> children;
	children.push_back(CompatConstant(Value(table_name)));
	table_function->function = make_uniq<FunctionExpression>("read_yaml", std::move(children));

	if (!FileSystem::HasGlob(table_name)) {
		auto &fs = FileSystem::GetFileSystem(context);
		table_function->alias = CompatMakeIdentifier(fs.ExtractBaseName(table_name));
	}

	return std::move(table_function);
}

void YAMLReader::RegisterFunction(ExtensionLoader &loader) {
	// Create read_yaml table function
	TableFunction read_yaml("read_yaml", {LogicalType::ANY}, YAMLReadRowsFunction, YAMLReadRowsBind, YAMLReadRowsInit);
	read_yaml.init_local = YAMLReadRowsInitLocal;
	read_yaml.get_partition_data = YAMLReadGetPartitionData;

	// Add optional named parameters
	CompatAddNamedParameter(read_yaml, "auto_detect", LogicalType::BOOLEAN);
	CompatAddNamedParameter(read_yaml, "ignore_errors", LogicalType::BOOLEAN);
	CompatAddNamedParameter(read_yaml, "maximum_object_size", LogicalType::BIGINT);
	CompatAddNamedParameter(read_yaml, "maximum_file_size", LogicalType::BIGINT);
	CompatAddNamedParameter(read_yaml, "multi_document", LogicalType::ANY); // Accepts BOOLEAN or VARCHAR for mode
	CompatAddNamedParameter(read_yaml, "expand_root_sequence", LogicalType::BOOLEAN);
	CompatAddNamedParameter(read_yaml, "columns", LogicalType::ANY);
	CompatAddNamedParameter(read_yaml, "sample_size", LogicalType::BIGINT);
	CompatAddNamedParameter(read_yaml, "maximum_sample_files", LogicalType::BIGINT);
	CompatAddNamedParameter(read_yaml, "records", LogicalType::VARCHAR);
	CompatAddNamedParameter(read_yaml, "frontmatter_as_columns", LogicalType::BOOLEAN);
	CompatAddNamedParameter(read_yaml, "list_column_name", LogicalType::VARCHAR);
	CompatAddNamedParameter(read_yaml, "strip_document_suffixes", LogicalType::BOOLEAN);

	{
		CreateTableFunctionInfo info(std::move(read_yaml));
		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		FunctionDescription desc;
		desc.parameter_names = {"path"};
		desc.description = "Read YAML files into a tabular format.";
		desc.examples = {"SELECT * FROM read_yaml('data.yaml')"};
		desc.categories = {"yaml"};
		info.descriptions.push_back(desc);
		loader.RegisterFunction(std::move(info));
	}

	// Register the object-based reader
	TableFunction read_yaml_objects("read_yaml_objects", {LogicalType::ANY}, YAMLReadObjectsFunction,
	                                YAMLReadObjectsBind, YAMLReadObjectsInit);
	read_yaml_objects.init_local = YAMLReadObjectsInitLocal;
	read_yaml_objects.get_partition_data = YAMLReadGetPartitionData;

	CompatAddNamedParameter(read_yaml_objects, "auto_detect", LogicalType::BOOLEAN);
	CompatAddNamedParameter(read_yaml_objects, "ignore_errors", LogicalType::BOOLEAN);
	CompatAddNamedParameter(read_yaml_objects, "maximum_object_size", LogicalType::BIGINT);
	CompatAddNamedParameter(read_yaml_objects, "maximum_file_size", LogicalType::BIGINT);
	CompatAddNamedParameter(read_yaml_objects, "multi_document",
	                        LogicalType::ANY); // Accepts BOOLEAN or VARCHAR for mode
	CompatAddNamedParameter(read_yaml_objects, "columns", LogicalType::ANY);
	CompatAddNamedParameter(read_yaml_objects, "sample_size", LogicalType::BIGINT);
	CompatAddNamedParameter(read_yaml_objects, "maximum_sample_files", LogicalType::BIGINT);
	CompatAddNamedParameter(read_yaml_objects, "strip_document_suffixes", LogicalType::BOOLEAN);

	{
		CreateTableFunctionInfo info(std::move(read_yaml_objects));
		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		FunctionDescription desc;
		desc.parameter_names = {"path"};
		desc.description = "Read YAML files as structured YAML objects.";
		desc.examples = {"SELECT * FROM read_yaml_objects('data.yaml')"};
		desc.categories = {"yaml"};
		info.descriptions.push_back(desc);
		loader.RegisterFunction(std::move(info));
	}

	// Register parse_yaml table function for parsing YAML strings
	TableFunction parse_yaml("parse_yaml", {LogicalType::VARCHAR}, ParseYAMLFunction, ParseYAMLBind);
	parse_yaml.init_local = ParseYAMLInit;
	CompatAddNamedParameter(parse_yaml, "multi_document", LogicalType::ANY); // Accepts BOOLEAN or VARCHAR for mode
	CompatAddNamedParameter(parse_yaml, "expand_root_sequence", LogicalType::BOOLEAN);
	CompatAddNamedParameter(parse_yaml, "frontmatter_as_columns", LogicalType::BOOLEAN);
	CompatAddNamedParameter(parse_yaml, "list_column_name", LogicalType::VARCHAR);

	{
		CreateTableFunctionInfo info(std::move(parse_yaml));
		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		FunctionDescription desc;
		desc.parameter_names = {"yaml_string"};
		desc.description = "Parse a YAML string into a table.";
		desc.examples = {"SELECT * FROM parse_yaml('a: 1\nb: 2')"};
		desc.categories = {"yaml"};
		info.descriptions.push_back(desc);
		loader.RegisterFunction(std::move(info));
	}
}

} // namespace duckdb
