#include "yaml_extraction_functions.hpp"
#include "yaml_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/common/types/vector.hpp"
#include "yaml-cpp/yaml.h"

namespace duckdb {

//===--------------------------------------------------------------------===//
// YAML Path Parsing
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
// YAML Type Functions
//===--------------------------------------------------------------------===//

static void YAMLTypeUnaryFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	UnaryExecutor::Execute<string_t, string_t>(args.data[0], result, args.size(), [&](string_t yaml_str) -> string_t {
		if (yaml_str.GetSize() == 0) {
			return StringVector::AddString(result, "null", 4);
		}

		try {
			YAML::Node node = YAML::Load(yaml_str.GetString());
			string type_str;

			switch (node.Type()) {
			case YAML::NodeType::Null:
				type_str = "null";
				break;
			case YAML::NodeType::Scalar:
				// Try to detect scalar subtype
				type_str = "scalar";
				break;
			case YAML::NodeType::Sequence:
				type_str = "array";
				break;
			case YAML::NodeType::Map:
				type_str = "object";
				break;
			default:
				type_str = "undefined";
				break;
			}

			return StringVector::AddString(result, type_str.c_str(), type_str.length());
		} catch (const std::exception &e) {
			throw InvalidInputException("Error parsing YAML: %s", e.what());
		}
	});
}

static void YAMLTypeBinaryFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	BinaryExecutor::ExecuteWithNulls<string_t, string_t, string_t>(
	    args.data[0], args.data[1], result, args.size(),
	    [&](string_t yaml_str, string_t path_str, ValidityMask &mask, idx_t idx) -> string_t {
		    if (yaml_str.GetSize() == 0) {
			    return StringVector::AddString(result, "null", 4);
		    }

		    try {
			    YAML::Node root = YAML::Load(yaml_str.GetString());
			    auto path_components = ParseYAMLPath(path_str.GetString());
			    auto node = ExtractFromYAML(root, path_components);

			    string type_str;
			    if (!node) {
				    // Nonexistent path - return SQL NULL
				    mask.SetInvalid(idx);
				    return string_t();
			    } else {
				    switch (node.Type()) {
				    case YAML::NodeType::Null:
					    type_str = "null";
					    break;
				    case YAML::NodeType::Scalar:
					    type_str = "scalar";
					    break;
				    case YAML::NodeType::Sequence:
					    type_str = "array";
					    break;
				    case YAML::NodeType::Map:
					    type_str = "object";
					    break;
				    default:
					    type_str = "undefined";
					    break;
				    }
			    }

			    return StringVector::AddString(result, type_str.c_str(), type_str.length());
		    } catch (const std::exception &e) {
			    throw InvalidInputException("Error in yaml_type: %s", e.what());
		    }
	    });
}

//===--------------------------------------------------------------------===//
// YAML Extract Functions
//===--------------------------------------------------------------------===//

static void YAMLExtractFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	BinaryExecutor::ExecuteWithNulls<string_t, string_t, string_t>(
	    args.data[0], args.data[1], result, args.size(),
	    [&](string_t yaml_str, string_t path_str, ValidityMask &mask, idx_t idx) -> string_t {
		    if (yaml_str.GetSize() == 0) {
			    return StringVector::AddString(result, "null", 4);
		    }

		    try {
			    YAML::Node root = YAML::Load(yaml_str.GetString());
			    auto path_components = ParseYAMLPath(path_str.GetString());
			    auto node = ExtractFromYAML(root, path_components);

			    // Convert extracted node back to YAML
			    if (!node) {
				    // Nonexistent path - return SQL NULL
				    mask.SetInvalid(idx);
				    return string_t();
			    }

			    YAML::Emitter out;
			    out.SetIndent(2);
			    out.SetMapFormat(YAML::Flow);
			    out.SetSeqFormat(YAML::Flow);
			    out << node;

			    string yaml_result = out.c_str();
			    return StringVector::AddString(result, yaml_result.c_str(), yaml_result.length());
		    } catch (const std::exception &e) {
			    throw InvalidInputException("Error in yaml_extract: %s", e.what());
		    }
	    });
}

static void YAMLExtractStringFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	BinaryExecutor::ExecuteWithNulls<string_t, string_t, string_t>(
	    args.data[0], args.data[1], result, args.size(),
	    [&](string_t yaml_str, string_t path_str, ValidityMask &mask, idx_t idx) -> string_t {
		    if (yaml_str.GetSize() == 0) {
			    mask.SetInvalid(idx);
			    return string_t();
		    }

		    try {
			    YAML::Node root = YAML::Load(yaml_str.GetString());
			    auto path_components = ParseYAMLPath(path_str.GetString());
			    auto node = ExtractFromYAML(root, path_components);

			    if (!node) {
				    // Nonexistent path - return SQL NULL
				    mask.SetInvalid(idx);
				    return string_t();
			    }

			    if (node.IsNull()) {
				    // YAML null value - return SQL NULL (same behavior as JSON)
				    mask.SetInvalid(idx);
				    return string_t();
			    }

			    if (!node.IsScalar()) {
				    // Convert non-scalar to YAML string representation
				    YAML::Emitter out;
				    out.SetIndent(2);
				    out.SetMapFormat(YAML::Flow);
				    out.SetSeqFormat(YAML::Flow);
				    out << node;
				    string yaml_result = out.c_str();
				    return StringVector::AddString(result, yaml_result.c_str(), yaml_result.length());
			    }

			    string str_val = node.Scalar();
			    return StringVector::AddString(result, str_val.c_str(), str_val.length());
		    } catch (const std::exception &e) {
			    throw InvalidInputException("Error in yaml_extract_string: %s", e.what());
		    }
	    });
}

//===--------------------------------------------------------------------===//
// YAML Exists Function
//===--------------------------------------------------------------------===//

static void YAMLExistsFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	BinaryExecutor::Execute<string_t, string_t, bool>(
	    args.data[0], args.data[1], result, args.size(), [&](string_t yaml_str, string_t path_str) -> bool {
		    if (yaml_str.GetSize() == 0) {
			    return false;
		    }

		    try {
			    YAML::Node root = YAML::Load(yaml_str.GetString());
			    auto path_components = ParseYAMLPath(path_str.GetString());
			    auto node = ExtractFromYAML(root, path_components);

			    return node.IsDefined() && !node.IsNull();
		    } catch (...) {
			    return false;
		    }
	    });
}

//===--------------------------------------------------------------------===//
// YAML Structure Function
//===--------------------------------------------------------------------===//

// Helper to detect the DuckDB type name for a scalar value
static string DetectScalarTypeName(const string &scalar_value) {
	if (scalar_value.empty() || scalar_value == "null" || scalar_value == "~") {
		return "NULL";
	}

	// Check for boolean
	string lower_value = scalar_value;
	std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
	if (lower_value == "true" || lower_value == "false" || lower_value == "yes" || lower_value == "no" ||
	    lower_value == "on" || lower_value == "off" || lower_value == "y" || lower_value == "n" ||
	    lower_value == "t" || lower_value == "f") {
		return "BOOLEAN";
	}

	// Check for special float values
	if (lower_value == "inf" || lower_value == "infinity" || lower_value == "-inf" || lower_value == "-infinity" ||
	    lower_value == "nan") {
		return "DOUBLE";
	}

	// Try integer
	try {
		size_t pos;
		int64_t int_val = std::stoll(scalar_value, &pos);
		if (pos == scalar_value.size()) {
			if (int_val >= 0) {
				return "UBIGINT";
			}
			return "BIGINT";
		}
	} catch (...) {
	}

	// Try double
	try {
		size_t pos;
		std::stod(scalar_value, &pos);
		if (pos == scalar_value.size()) {
			return "DOUBLE";
		}
	} catch (...) {
	}

	return "VARCHAR";
}

// Forward declaration for mutual recursion
static string BuildYAMLStructure(const YAML::Node &node);

// Helper to escape a key for JSON output
static string EscapeJSONKey(const string &key) {
	string escaped_key;
	for (char c : key) {
		if (c == '"') {
			escaped_key += "\\\"";
		} else if (c == '\\') {
			escaped_key += "\\\\";
		} else {
			escaped_key += c;
		}
	}
	return escaped_key;
}

// Helper to merge two object structures (for arrays of objects with different keys)
static string MergeObjectStructures(const YAML::Node &nodes_array) {
	// Collect all keys and their types from all objects in the array
	unordered_map<string, string> merged_keys;

	for (size_t i = 0; i < nodes_array.size(); i++) {
		const auto &elem = nodes_array[i];
		if (elem.IsMap()) {
			for (auto it = elem.begin(); it != elem.end(); ++it) {
				string key = it->first.Scalar();
				string value_structure = BuildYAMLStructure(it->second);
				// If key already exists, keep the existing type (or could merge types)
				if (merged_keys.find(key) == merged_keys.end()) {
					merged_keys[key] = value_structure;
				}
			}
		}
	}

	// Build the merged object structure
	string result = "{";
	bool first = true;
	for (const auto &kv : merged_keys) {
		if (!first) {
			result += ",";
		}
		first = false;
		result += "\"" + EscapeJSONKey(kv.first) + "\":" + kv.second;
	}
	result += "}";
	return result;
}

// Recursively build structure JSON for a YAML node
static string BuildYAMLStructure(const YAML::Node &node) {
	if (!node || node.IsNull()) {
		return "\"NULL\"";
	}

	switch (node.Type()) {
	case YAML::NodeType::Scalar: {
		string type_name = DetectScalarTypeName(node.Scalar());
		return "\"" + type_name + "\"";
	}
	case YAML::NodeType::Sequence: {
		if (node.size() == 0) {
			return "[\"NULL\"]";
		}

		// Check if all elements are maps - if so, merge their structures
		bool all_maps = true;
		for (size_t i = 0; i < node.size(); i++) {
			if (!node[i].IsMap()) {
				all_maps = false;
				break;
			}
		}

		if (all_maps) {
			// Merge all object structures
			return "[" + MergeObjectStructures(node) + "]";
		}

		// For non-object arrays, use the first element's structure
		string first_structure = BuildYAMLStructure(node[0]);
		return "[" + first_structure + "]";
	}
	case YAML::NodeType::Map: {
		string result = "{";
		bool first = true;
		for (auto it = node.begin(); it != node.end(); ++it) {
			if (!first) {
				result += ",";
			}
			first = false;

			string key = it->first.Scalar();
			result += "\"" + EscapeJSONKey(key) + "\":" + BuildYAMLStructure(it->second);
		}
		result += "}";
		return result;
	}
	default:
		return "\"NULL\"";
	}
}

static void YAMLStructureFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	UnaryExecutor::Execute<string_t, string_t>(args.data[0], result, args.size(), [&](string_t yaml_str) -> string_t {
		if (yaml_str.GetSize() == 0) {
			return StringVector::AddString(result, "\"NULL\"");
		}

		try {
			YAML::Node root = YAML::Load(yaml_str.GetString());
			string structure = BuildYAMLStructure(root);
			return StringVector::AddString(result, structure);
		} catch (const std::exception &e) {
			throw InvalidInputException("Error in yaml_structure: %s", e.what());
		}
	});
}

//===--------------------------------------------------------------------===//
// Registration
//===--------------------------------------------------------------------===//

// Helper to register a function with multiple names (for aliases like -> and ->>)
template <class FUNCTION_INFO>
static void AddAliases(const vector<string> &names, FUNCTION_INFO fun, vector<FUNCTION_INFO> &functions) {
	for (const auto &name : names) {
		fun.name = name;
		functions.push_back(fun);
	}
}

void YAMLExtractionFunctions::Register(ExtensionLoader &loader) {
	// Get the YAML type
	auto yaml_type = YAMLTypes::YAMLType();

	// yaml_type function
	ScalarFunctionSet yaml_type_set("yaml_type");
	yaml_type_set.AddFunction(ScalarFunction({yaml_type}, LogicalType::VARCHAR, YAMLTypeUnaryFunction));
	yaml_type_set.AddFunction(
	    ScalarFunction({yaml_type, LogicalType::VARCHAR}, LogicalType::VARCHAR, YAMLTypeBinaryFunction));
	// Also support VARCHAR input
	yaml_type_set.AddFunction(ScalarFunction({LogicalType::VARCHAR}, LogicalType::VARCHAR, YAMLTypeUnaryFunction));
	yaml_type_set.AddFunction(
	    ScalarFunction({LogicalType::VARCHAR, LogicalType::VARCHAR}, LogicalType::VARCHAR, YAMLTypeBinaryFunction));
	loader.RegisterFunction(yaml_type_set);

	// yaml_extract function
	// Returns YAML type, accepts both YAML and VARCHAR input
	// Note: The -> operator cannot be aliased because DuckDB's planner hardcodes it to json_extract
	ScalarFunctionSet yaml_extract_set("yaml_extract");
	yaml_extract_set.AddFunction(ScalarFunction({yaml_type, LogicalType::VARCHAR}, yaml_type, YAMLExtractFunction));
	yaml_extract_set.AddFunction(
	    ScalarFunction({LogicalType::VARCHAR, LogicalType::VARCHAR}, yaml_type, YAMLExtractFunction));
	loader.RegisterFunction(yaml_extract_set);

	// yaml_extract_string function with ->> alias
	// Returns VARCHAR, accepts both YAML and VARCHAR input
	ScalarFunctionSet yaml_extract_string_set("yaml_extract_string");
	yaml_extract_string_set.AddFunction(
	    ScalarFunction({yaml_type, LogicalType::VARCHAR}, LogicalType::VARCHAR, YAMLExtractStringFunction));
	yaml_extract_string_set.AddFunction(
	    ScalarFunction({LogicalType::VARCHAR, LogicalType::VARCHAR}, LogicalType::VARCHAR, YAMLExtractStringFunction));

	// Register yaml_extract_string and ->> alias
	vector<ScalarFunctionSet> extract_string_functions;
	AddAliases({"yaml_extract_string", "->>"}, yaml_extract_string_set, extract_string_functions);
	for (auto &func : extract_string_functions) {
		loader.RegisterFunction(func);
	}

	// yaml_exists function
	ScalarFunctionSet yaml_exists_set("yaml_exists");
	yaml_exists_set.AddFunction(
	    ScalarFunction({yaml_type, LogicalType::VARCHAR}, LogicalType::BOOLEAN, YAMLExistsFunction));
	yaml_exists_set.AddFunction(
	    ScalarFunction({LogicalType::VARCHAR, LogicalType::VARCHAR}, LogicalType::BOOLEAN, YAMLExistsFunction));
	loader.RegisterFunction(yaml_exists_set);

	// yaml_structure function - returns JSON representation of YAML structure
	ScalarFunctionSet yaml_structure_set("yaml_structure");
	yaml_structure_set.AddFunction(ScalarFunction({yaml_type}, LogicalType::JSON(), YAMLStructureFunction));
	yaml_structure_set.AddFunction(ScalarFunction({LogicalType::VARCHAR}, LogicalType::JSON(), YAMLStructureFunction));
	loader.RegisterFunction(yaml_structure_set);
}

} // namespace duckdb
