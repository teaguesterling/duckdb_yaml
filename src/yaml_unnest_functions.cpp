#include "yaml_unnest_functions.hpp"
#include "yaml_types.hpp"
#include "yaml_utils.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/common/types/vector.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/function/aggregate_function.hpp"
#include "yaml-cpp/yaml.h"

namespace duckdb {

//===--------------------------------------------------------------------===//
// YAML Path Parsing (reused from yaml_extraction_functions.cpp)
//===--------------------------------------------------------------------===//

static vector<string> ParseYAMLPath(const string &path) {
	vector<string> components;
	if (path.empty() || path[0] != '$') {
		throw InvalidInputException("YAML path must start with '$'");
	}

	string current_component;
	bool in_quotes = false;
	bool escaped = false;

	for (size_t idx = 1; idx < path.length(); idx++) {
		char c = path[idx];

		if (escaped) {
			current_component += c;
			escaped = false;
			continue;
		}

		if (c == '\\') {
			escaped = true;
			continue;
		}

		if (c == '\'' || c == '"') {
			in_quotes = !in_quotes;
			continue;
		}

		if (!in_quotes && (c == '.' || c == '[')) {
			if (!current_component.empty()) {
				components.push_back(current_component);
				current_component.clear();
			}

			if (c == '[') {
				// Handle array index
				size_t j = idx + 1;
				while (j < path.length() && path[j] != ']') {
					j++;
				}
				if (j >= path.length()) {
					throw InvalidInputException("Unclosed array index in path");
				}
				components.push_back(path.substr(idx, j - idx + 1));
				idx = j;
			}
			continue;
		}

		current_component += c;
	}

	if (!current_component.empty()) {
		components.push_back(current_component);
	}

	return components;
}

static YAML::Node ExtractFromYAML(const YAML::Node &node, const vector<string> &path_components, size_t index = 0) {
	if (index >= path_components.size()) {
		return node;
	}

	const string &component = path_components[index];

	// Handle array index
	if (!component.empty() && component[0] == '[') {
		if (!node.IsSequence()) {
			return YAML::Node(); // Return null node
		}

		string index_str = component.substr(1, component.length() - 2);
		try {
			size_t arr_index = std::stoul(index_str);
			if (arr_index >= node.size()) {
				return YAML::Node();
			}
			return ExtractFromYAML(node[arr_index], path_components, index + 1);
		} catch (...) {
			throw InvalidInputException("Invalid array index: %s", index_str);
		}
	}

	// Handle map key
	if (!node.IsMap()) {
		return YAML::Node();
	}

	auto child = node[component];
	return ExtractFromYAML(child, path_components, index + 1);
}

//===--------------------------------------------------------------------===//
// yaml_array_length
//===--------------------------------------------------------------------===//

static void YAMLArrayLengthUnaryFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	UnaryExecutor::ExecuteWithNulls<string_t, int64_t>(
	    args.data[0], result, args.size(), [&](string_t yaml_str, ValidityMask &mask, idx_t idx) -> int64_t {
		    if (yaml_str.GetSize() == 0) {
			    mask.SetInvalid(idx);
			    return 0;
		    }

		    try {
			    YAML::Node node = YAML::Load(yaml_str.GetString());

			    if (!node.IsSequence()) {
				    // Not an array - return NULL
				    mask.SetInvalid(idx);
				    return 0;
			    }

			    return static_cast<int64_t>(node.size());
		    } catch (const std::exception &e) {
			    throw InvalidInputException("Error parsing YAML: %s", e.what());
		    }
	    });
}

static void YAMLArrayLengthBinaryFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	BinaryExecutor::ExecuteWithNulls<string_t, string_t, int64_t>(
	    args.data[0], args.data[1], result, args.size(),
	    [&](string_t yaml_str, string_t path_str, ValidityMask &mask, idx_t idx) -> int64_t {
		    if (yaml_str.GetSize() == 0) {
			    mask.SetInvalid(idx);
			    return 0;
		    }

		    try {
			    YAML::Node root = YAML::Load(yaml_str.GetString());
			    auto path_components = ParseYAMLPath(path_str.GetString());
			    auto node = ExtractFromYAML(root, path_components);

			    if (!node || !node.IsSequence()) {
				    // Path doesn't exist or not an array - return NULL
				    mask.SetInvalid(idx);
				    return 0;
			    }

			    return static_cast<int64_t>(node.size());
		    } catch (const std::exception &e) {
			    throw InvalidInputException("Error in yaml_array_length: %s", e.what());
		    }
	    });
}

//===--------------------------------------------------------------------===//
// yaml_keys
//===--------------------------------------------------------------------===//

static void YAMLKeysUnaryFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	UnaryExecutor::ExecuteWithNulls<string_t, list_entry_t>(
	    args.data[0], result, args.size(), [&](string_t yaml_str, ValidityMask &mask, idx_t idx) -> list_entry_t {
		    if (yaml_str.GetSize() == 0) {
			    mask.SetInvalid(idx);
			    return {0, 0};
		    }

		    try {
			    YAML::Node node = YAML::Load(yaml_str.GetString());

			    if (!node.IsMap()) {
				    // Not an object - return NULL
				    mask.SetInvalid(idx);
				    return {0, 0};
			    }

			    // Collect all keys
			    vector<Value> keys;
			    for (auto it = node.begin(); it != node.end(); ++it) {
				    string key = it->first.Scalar();
				    keys.push_back(Value(key));
			    }

			    // Create list value
			    auto list_value = Value::LIST(LogicalType::VARCHAR, keys);

			    // Get the list child vector
			    auto &child_vector = ListVector::GetEntry(result);
			    auto list_data = FlatVector::GetData<string_t>(child_vector);

			    // Add strings to child vector
			    list_entry_t entry;
			    entry.offset = ListVector::GetListSize(result);
			    entry.length = keys.size();

			    for (size_t i = 0; i < keys.size(); i++) {
				    auto key_str = keys[i].GetValue<string>();
				    list_data[entry.offset + i] = StringVector::AddString(child_vector, key_str);
			    }

			    ListVector::SetListSize(result, entry.offset + entry.length);
			    return entry;
		    } catch (const std::exception &e) {
			    throw InvalidInputException("Error parsing YAML: %s", e.what());
		    }
	    });
}

static void YAMLKeysBinaryFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	BinaryExecutor::ExecuteWithNulls<string_t, string_t, list_entry_t>(
	    args.data[0], args.data[1], result, args.size(),
	    [&](string_t yaml_str, string_t path_str, ValidityMask &mask, idx_t idx) -> list_entry_t {
		    if (yaml_str.GetSize() == 0) {
			    mask.SetInvalid(idx);
			    return {0, 0};
		    }

		    try {
			    YAML::Node root = YAML::Load(yaml_str.GetString());
			    auto path_components = ParseYAMLPath(path_str.GetString());
			    auto node = ExtractFromYAML(root, path_components);

			    if (!node || !node.IsMap()) {
				    // Path doesn't exist or not an object - return NULL
				    mask.SetInvalid(idx);
				    return {0, 0};
			    }

			    // Collect all keys
			    vector<Value> keys;
			    for (auto it = node.begin(); it != node.end(); ++it) {
				    string key = it->first.Scalar();
				    keys.push_back(Value(key));
			    }

			    // Get the list child vector
			    auto &child_vector = ListVector::GetEntry(result);
			    auto list_data = FlatVector::GetData<string_t>(child_vector);

			    // Add strings to child vector
			    list_entry_t entry;
			    entry.offset = ListVector::GetListSize(result);
			    entry.length = keys.size();

			    for (size_t i = 0; i < keys.size(); i++) {
				    auto key_str = keys[i].GetValue<string>();
				    list_data[entry.offset + i] = StringVector::AddString(child_vector, key_str);
			    }

			    ListVector::SetListSize(result, entry.offset + entry.length);
			    return entry;
		    } catch (const std::exception &e) {
			    throw InvalidInputException("Error in yaml_keys: %s", e.what());
		    }
	    });
}

//===--------------------------------------------------------------------===//
// yaml_array_elements - Table Function
//===--------------------------------------------------------------------===//

struct YAMLArrayElementsBindData : public TableFunctionData {
	vector<string> elements; // Pre-converted YAML strings
	mutable idx_t current_idx = 0;
};

static unique_ptr<FunctionData> YAMLArrayElementsBind(ClientContext &context, TableFunctionBindInput &input,
                                                      vector<LogicalType> &return_types, vector<string> &names) {
	if (input.inputs.empty()) {
		throw BinderException("yaml_array_elements requires a YAML array parameter");
	}

	auto result = make_uniq<YAMLArrayElementsBindData>();

	// Parse the YAML during bind time
	Value yaml_value = input.inputs[0];
	if (!yaml_value.IsNull()) {
		string yaml_str = yaml_value.ToString();

		try {
			YAML::Node node = YAML::Load(yaml_str);

			if (!node.IsSequence()) {
				throw BinderException("yaml_array_elements requires a YAML array");
			}

			// Store all elements as YAML strings
			for (size_t i = 0; i < node.size(); i++) {
				YAML::Emitter out;
				out.SetIndent(2);
				out.SetMapFormat(YAML::Flow);
				out.SetSeqFormat(YAML::Flow);
				out << node[i];
				result->elements.push_back(out.c_str());
			}
		} catch (const YAML::Exception &e) {
			throw BinderException("Error parsing YAML: %s", e.what());
		}
	}

	// Set up output schema
	names.push_back("value");
	return_types.push_back(YAMLTypes::YAMLType());

	return std::move(result);
}

static void YAMLArrayElementsFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->Cast<YAMLArrayElementsBindData>();

	// Check if we're done
	if (bind_data.current_idx >= bind_data.elements.size()) {
		output.SetCardinality(0);
		return;
	}

	// Output rows
	idx_t count = 0;
	idx_t max_count = std::min((idx_t)STANDARD_VECTOR_SIZE, bind_data.elements.size() - bind_data.current_idx);

	output.Reset();

	for (idx_t i = 0; i < max_count; i++) {
		const auto &yaml_str = bind_data.elements[bind_data.current_idx + i];
		output.SetValue(0, count, Value(yaml_str));
		count++;
	}

	bind_data.current_idx += count;
	output.SetCardinality(count);
}

//===--------------------------------------------------------------------===//
// yaml_each - Table Function
//===--------------------------------------------------------------------===//

struct YAMLEachBindData : public TableFunctionData {
	vector<pair<string, string>> entries; // Pre-converted key-value pairs
	mutable idx_t current_idx = 0;
};

static unique_ptr<FunctionData> YAMLEachBind(ClientContext &context, TableFunctionBindInput &input,
                                             vector<LogicalType> &return_types, vector<string> &names) {
	if (input.inputs.empty()) {
		throw BinderException("yaml_each requires a YAML object parameter");
	}

	auto result = make_uniq<YAMLEachBindData>();

	// Parse the YAML during bind time
	Value yaml_value = input.inputs[0];
	if (!yaml_value.IsNull()) {
		string yaml_str = yaml_value.ToString();

		try {
			YAML::Node node = YAML::Load(yaml_str);

			if (!node.IsMap()) {
				throw BinderException("yaml_each requires a YAML object");
			}

			// Store all key-value pairs as strings
			for (auto it = node.begin(); it != node.end(); ++it) {
				string key = it->first.Scalar();

				YAML::Emitter out;
				out.SetIndent(2);
				out.SetMapFormat(YAML::Flow);
				out.SetSeqFormat(YAML::Flow);
				out << it->second;

				result->entries.push_back({key, out.c_str()});
			}
		} catch (const YAML::Exception &e) {
			throw BinderException("Error parsing YAML: %s", e.what());
		}
	}

	// Set up output schema
	names.push_back("key");
	names.push_back("value");
	return_types.push_back(LogicalType::VARCHAR);
	return_types.push_back(YAMLTypes::YAMLType());

	return std::move(result);
}

static void YAMLEachFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->Cast<YAMLEachBindData>();

	// Check if we're done
	if (bind_data.current_idx >= bind_data.entries.size()) {
		output.SetCardinality(0);
		return;
	}

	// Output rows
	idx_t count = 0;
	idx_t max_count = std::min((idx_t)STANDARD_VECTOR_SIZE, bind_data.entries.size() - bind_data.current_idx);

	output.Reset();

	for (idx_t i = 0; i < max_count; i++) {
		const auto &entry = bind_data.entries[bind_data.current_idx + i];

		// Set key
		output.SetValue(0, count, Value(entry.first));

		// Set value
		output.SetValue(1, count, Value(entry.second));
		count++;
	}

	bind_data.current_idx += count;
	output.SetCardinality(count);
}

//===--------------------------------------------------------------------===//
// yaml_build_object
//===--------------------------------------------------------------------===//

static void YAMLBuildObjectFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	// yaml_build_object expects alternating key-value pairs
	if (args.ColumnCount() == 0) {
		// Empty object
		for (idx_t row_idx = 0; row_idx < args.size(); row_idx++) {
			result.SetValue(row_idx, Value("{}"));
		}
		return;
	}

	if (args.ColumnCount() % 2 != 0) {
		throw InvalidInputException("yaml_build_object requires an even number of arguments (key-value pairs)");
	}

	// Process each row
	for (idx_t row_idx = 0; row_idx < args.size(); row_idx++) {
		YAML::Node obj_node;

		// Process each key-value pair
		for (idx_t arg_idx = 0; arg_idx < args.ColumnCount(); arg_idx += 2) {
			// Get key
			Value key_value = args.data[arg_idx].GetValue(row_idx);
			if (key_value.IsNull()) {
				throw InvalidInputException("yaml_build_object keys cannot be NULL");
			}
			string key = key_value.ToString();

			// Get value
			Value value_val = args.data[arg_idx + 1].GetValue(row_idx);

			// Convert value to YAML node
			if (value_val.IsNull()) {
				obj_node[key] = YAML::Node(YAML::NodeType::Null);
			} else {
				// Try to parse as YAML first (if it's a YAML type)
				string val_str = value_val.ToString();
				try {
					YAML::Node parsed = YAML::Load(val_str);
					obj_node[key] = parsed;
				} catch (...) {
					// If parsing fails, treat as scalar
					obj_node[key] = val_str;
				}
			}
		}

		// Convert to YAML string
		YAML::Emitter out;
		out.SetIndent(2);
		out.SetMapFormat(YAML::Flow);
		out.SetSeqFormat(YAML::Flow);
		out << obj_node;

		string yaml_str = out.c_str();
		result.SetValue(row_idx, Value(yaml_str));
	}
}

//===--------------------------------------------------------------------===//
// yaml_agg - Aggregate Function
//===--------------------------------------------------------------------===//

struct YAMLAggState {
	idx_t count;      // Number of elements
	idx_t size;       // Total size in bytes
	idx_t alloc_size; // Allocated size
	char *dataptr;    // Buffer containing all YAML strings separated by null bytes
};

struct YAMLAggFunction {
	template <class STATE>
	static void Initialize(STATE &state) {
		state.count = 0;
		state.size = 0;
		state.alloc_size = 0;
		state.dataptr = nullptr;
	}

	static bool IgnoreNull() {
		return true;
	}
};

static void YAMLAggStoreString(YAMLAggState &state, const char *input_data, idx_t input_size,
                               ArenaAllocator &allocator) {
	// Calculate required size (string + null terminator)
	idx_t required_size = state.size + input_size + 1;

	if (!state.dataptr) {
		// First iteration: allocate space
		state.alloc_size = MaxValue<idx_t>(1024, NextPowerOfTwo(required_size));
		state.dataptr = char_ptr_cast(allocator.Allocate(state.alloc_size));
		state.size = 0;
	} else if (required_size > state.alloc_size) {
		// Need more space: reallocate
		const auto old_size = state.alloc_size;
		while (state.alloc_size < required_size) {
			state.alloc_size *= 2;
		}
		state.dataptr = char_ptr_cast(allocator.Reallocate(data_ptr_cast(state.dataptr), old_size, state.alloc_size));
	}

	// Copy the string and add null terminator
	memcpy(state.dataptr + state.size, input_data, input_size);
	state.size += input_size;
	state.dataptr[state.size] = '\0';
	state.size += 1;
	state.count += 1;
}

static void YAMLAggUpdate(Vector inputs[], AggregateInputData &aggr_input_data, idx_t input_count, Vector &state_vector,
                          idx_t count) {
	D_ASSERT(input_count == 1);

	UnifiedVectorFormat input_data;
	inputs[0].ToUnifiedFormat(count, input_data);

	auto states = FlatVector::GetData<YAMLAggState *>(state_vector);

	for (idx_t i = 0; i < count; i++) {
		auto idx = input_data.sel->get_index(i);
		if (!input_data.validity.RowIsValid(idx)) {
			continue;
		}

		auto &state = *states[i];

		// Get the value and convert to YAML string
		Value val = inputs[0].GetValue(idx);
		string yaml_str = val.ToString();

		YAMLAggStoreString(state, yaml_str.c_str(), yaml_str.size(), aggr_input_data.allocator);
	}
}

static void YAMLAggCombine(Vector &state_vector, Vector &combined_vector, AggregateInputData &aggr_input_data,
                           idx_t count) {
	auto states_source = FlatVector::GetData<YAMLAggState *>(state_vector);
	auto states_target = FlatVector::GetData<YAMLAggState *>(combined_vector);

	for (idx_t i = 0; i < count; i++) {
		auto &source = *states_source[i];
		auto &target = *states_target[i];

		if (!source.dataptr || source.count == 0) {
			continue;
		}

		// Calculate required size
		idx_t required_size = target.size + source.size;

		if (!target.dataptr) {
			// First iteration: allocate space
			target.alloc_size = MaxValue<idx_t>(1024, NextPowerOfTwo(required_size));
			target.dataptr = char_ptr_cast(aggr_input_data.allocator.Allocate(target.alloc_size));
			target.size = 0;
			target.count = 0;
		} else if (required_size > target.alloc_size) {
			// Need more space: reallocate
			const auto old_size = target.alloc_size;
			while (target.alloc_size < required_size) {
				target.alloc_size *= 2;
			}
			target.dataptr = char_ptr_cast(
			    aggr_input_data.allocator.Reallocate(data_ptr_cast(target.dataptr), old_size, target.alloc_size));
		}

		// Copy all source data
		memcpy(target.dataptr + target.size, source.dataptr, source.size);
		target.size += source.size;
		target.count += source.count;
	}
}

static void YAMLAggFinalize(Vector &state_vector, AggregateInputData &aggr_input_data, Vector &result, idx_t count,
                            idx_t offset) {
	auto states = FlatVector::GetData<YAMLAggState *>(state_vector);

	for (idx_t i = 0; i < count; i++) {
		auto &state = *states[i];

		if (!state.dataptr || state.count == 0) {
			// Empty array
			result.SetValue(offset + i, Value("[]"));
			continue;
		}

		// Parse the buffer and build YAML array
		YAML::Node array_node;
		idx_t offset_idx = 0;

		for (idx_t j = 0; j < state.count; j++) {
			// Get the next null-terminated string
			const char *elem_str = state.dataptr + offset_idx;
			idx_t elem_len = strlen(elem_str);

			try {
				YAML::Node node = YAML::Load(elem_str);
				array_node.push_back(node);
			} catch (...) {
				// If parsing fails, treat as scalar string
				array_node.push_back(elem_str);
			}

			offset_idx += elem_len + 1;
		}

		// Convert to YAML string
		YAML::Emitter out;
		out.SetIndent(2);
		out.SetMapFormat(YAML::Flow);
		out.SetSeqFormat(YAML::Flow);
		out << array_node;

		result.SetValue(offset + i, Value(out.c_str()));
	}
}

//===--------------------------------------------------------------------===//
// Registration
//===--------------------------------------------------------------------===//

void YAMLUnnestFunctions::Register(ExtensionLoader &loader) {
	auto yaml_type = YAMLTypes::YAMLType();

	// yaml_array_length function
	ScalarFunctionSet yaml_array_length_set("yaml_array_length");
	yaml_array_length_set.AddFunction(ScalarFunction({yaml_type}, LogicalType::BIGINT, YAMLArrayLengthUnaryFunction));
	yaml_array_length_set.AddFunction(
	    ScalarFunction({yaml_type, LogicalType::VARCHAR}, LogicalType::BIGINT, YAMLArrayLengthBinaryFunction));
	loader.RegisterFunction(yaml_array_length_set);

	// yaml_keys function
	ScalarFunctionSet yaml_keys_set("yaml_keys");
	yaml_keys_set.AddFunction(
	    ScalarFunction({yaml_type}, LogicalType::LIST(LogicalType::VARCHAR), YAMLKeysUnaryFunction));
	yaml_keys_set.AddFunction(ScalarFunction({yaml_type, LogicalType::VARCHAR}, LogicalType::LIST(LogicalType::VARCHAR),
	                                         YAMLKeysBinaryFunction));
	loader.RegisterFunction(yaml_keys_set);

	// yaml_array_elements table function
	TableFunction yaml_array_elements("yaml_array_elements", {yaml_type}, YAMLArrayElementsFunction,
	                                  YAMLArrayElementsBind);
	loader.RegisterFunction(yaml_array_elements);

	// yaml_each table function
	TableFunction yaml_each("yaml_each", {yaml_type}, YAMLEachFunction, YAMLEachBind);
	loader.RegisterFunction(yaml_each);

	// yaml_build_object function - variadic function
	auto yaml_build_object_fun = ScalarFunction("yaml_build_object", {}, yaml_type, YAMLBuildObjectFunction);
	yaml_build_object_fun.varargs = LogicalType::ANY;
	loader.RegisterFunction(yaml_build_object_fun);

	// yaml_agg aggregate function
	auto yaml_agg_fun =
	    AggregateFunction("yaml_agg", {LogicalType::ANY}, yaml_type, AggregateFunction::StateSize<YAMLAggState>,
	                      AggregateFunction::StateInitialize<YAMLAggState, YAMLAggFunction>, YAMLAggUpdate,
	                      YAMLAggCombine, YAMLAggFinalize,
	                      nullptr, // simple_update
	                      nullptr, // bind
	                      nullptr  // destructor
	    );
	loader.RegisterFunction(yaml_agg_fun);
}

} // namespace duckdb
