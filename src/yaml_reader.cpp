#include "yaml_reader.hpp"
#include "duckdb/catalog/catalog_entry/table_function_catalog_entry.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include "duckdb/main/extension_util.hpp"

namespace duckdb {

unique_ptr<TableRef> YAMLReader::ReadYAMLReplacement(ClientContext &context, ReplacementScanInput &input,
                                                     optional_ptr<ReplacementScanData> data) {
	auto table_name = ReplacementScan::GetFullPath(input);
	if (!ReplacementScan::CanReplace(table_name, {"yaml", "yml"})) {
		return nullptr;
	}

	auto table_function = make_uniq<TableFunctionRef>();
	vector<unique_ptr<ParsedExpression>> children;
	children.push_back(make_uniq<ConstantExpression>(Value(table_name)));
	table_function->function = make_uniq<FunctionExpression>("read_yaml", std::move(children));

	if (!FileSystem::HasGlob(table_name)) {
		auto &fs = FileSystem::GetFileSystem(context);
		table_function->alias = fs.ExtractBaseName(table_name);
	}

	return std::move(table_function);
}

void YAMLReader::RegisterFunction(DatabaseInstance &db) {
	// Create read_yaml table function
	TableFunction read_yaml("read_yaml", {LogicalType::ANY}, YAMLReadRowsFunction, YAMLReadRowsBind);

	// Add optional named parameters
	read_yaml.named_parameters["auto_detect"] = LogicalType::BOOLEAN;
	read_yaml.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	read_yaml.named_parameters["maximum_object_size"] = LogicalType::BIGINT;
	read_yaml.named_parameters["multi_document"] = LogicalType::BOOLEAN;
	read_yaml.named_parameters["expand_root_sequence"] = LogicalType::BOOLEAN;
	read_yaml.named_parameters["columns"] = LogicalType::ANY;

	// Register the function
	ExtensionUtil::RegisterFunction(db, read_yaml);

	// Register the object-based reader
	TableFunction read_yaml_objects("read_yaml_objects", {LogicalType::ANY}, YAMLReadObjectsFunction,
	                                YAMLReadObjectsBind);
	read_yaml_objects.named_parameters["auto_detect"] = LogicalType::BOOLEAN;
	read_yaml_objects.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	read_yaml_objects.named_parameters["maximum_object_size"] = LogicalType::BIGINT;
	read_yaml_objects.named_parameters["multi_document"] = LogicalType::BOOLEAN;
	read_yaml_objects.named_parameters["columns"] = LogicalType::ANY;
	ExtensionUtil::RegisterFunction(db, read_yaml_objects);
}

} // namespace duckdb