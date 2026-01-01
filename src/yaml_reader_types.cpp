#include "yaml_reader.hpp"
#include "yaml_utils.hpp"
#include "duckdb/common/types/date.hpp"
#include "duckdb/common/types/timestamp.hpp"
#include "duckdb/common/types/time.hpp"
#include "duckdb/common/types.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace duckdb {

// YAML Type Conversions
// Helper function to detect YAML type
LogicalType YAMLReader::DetectYAMLType(const YAML::Node &node) {
	if (!node) {
		return LogicalType::VARCHAR;
	}

	switch (node.Type()) {
	case YAML::NodeType::Scalar: {
		std::string scalar_value = node.Scalar();

		// Check for null values
		if (scalar_value == "null" || scalar_value == "~" || scalar_value.empty()) {
			return LogicalType::VARCHAR; // Will be NULL in the actual data
		}

		// Check for boolean values (case-insensitive)
		std::string lower_value = scalar_value;
		std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);

		if (lower_value == "true" || lower_value == "false" || lower_value == "yes" || lower_value == "no" ||
		    lower_value == "on" || lower_value == "off" || lower_value == "y" || lower_value == "n" ||
		    lower_value == "t" || lower_value == "f") {
			return LogicalType::BOOLEAN;
		}

		// Skip numeric detection for values that might be dates/times
		bool might_be_temporal = false;
		if (scalar_value.find('-') != std::string::npos || scalar_value.find(':') != std::string::npos ||
		    scalar_value.find('T') != std::string::npos) {
			// Don't skip potential negative numbers - only temporal if it's not just a negative sign
			might_be_temporal =
			    (scalar_value.find(':') != std::string::npos || scalar_value.find('T') != std::string::npos) ||
			    (scalar_value.find('-') != std::string::npos && scalar_value[0] != '-');

			if (might_be_temporal) {
				// Try to parse as date
				idx_t pos = 0;
				date_t date_result;
				bool special = false;
				auto date_cast_result =
				    Date::TryConvertDate(scalar_value.c_str(), scalar_value.length(), pos, date_result, special, false);
				if (date_cast_result == DateCastResult::SUCCESS && pos == scalar_value.length()) {
					return LogicalType::DATE;
				}

				// Try to parse as timestamp
				timestamp_t timestamp_result;
				if (Timestamp::TryConvertTimestamp(scalar_value.c_str(), scalar_value.length(), timestamp_result,
				                                   false) == TimestampCastResult::SUCCESS) {
					return LogicalType::TIMESTAMP;
				}

				// Try to parse as time
				pos = 0;
				dtime_t time_result;
				if (Time::TryConvertTime(scalar_value.c_str(), scalar_value.length(), pos, time_result, false) &&
				    pos == scalar_value.length()) {
					return LogicalType::TIME;
				}
			}
		}

		if (!might_be_temporal) {
			// Check for special floating point values first
			if (lower_value == "inf" || lower_value == "infinity") {
				return LogicalType::DOUBLE;
			} else if (lower_value == "-inf" || lower_value == "-infinity") {
				return LogicalType::DOUBLE;
			} else if (lower_value == "nan") {
				return LogicalType::DOUBLE;
			}

			try {
				// Try integer
				size_t pos;
				int64_t int_val = std::stoll(scalar_value, &pos);
				if (pos == scalar_value.size()) {
					// Choose appropriate integer type based on value
					if (int_val >= -128 && int_val <= 127) {
						return LogicalType::TINYINT;
					} else if (int_val >= -32768 && int_val <= 32767) {
						return LogicalType::SMALLINT;
					} else if (int_val >= -2147483648LL && int_val <= 2147483647LL) {
						return LogicalType::INTEGER;
					} else {
						return LogicalType::BIGINT;
					}
				}

				// Try double
				double double_val = std::stod(scalar_value, &pos);
				if (pos == scalar_value.size()) {
					// Check for special floating point values
					if (std::isinf(double_val) || std::isnan(double_val)) {
						return LogicalType::DOUBLE;
					}
					// Check if it's a whole number that was written with decimal point
					if (double_val == std::floor(double_val) && double_val >= std::numeric_limits<int64_t>::min() &&
					    double_val <= std::numeric_limits<int64_t>::max()) {
						// It's a whole number, use integer type
						return DetectYAMLType(YAML::Node(std::to_string(static_cast<int64_t>(double_val))));
					}
					return LogicalType::DOUBLE;
				}
			} catch (...) {
				// Not a number
			}
		}
		return LogicalType::VARCHAR;
	}
	case YAML::NodeType::Sequence: {
		if (node.size() == 0) {
			return LogicalType::LIST(LogicalType::VARCHAR);
		}

		// For mixed-type sequences, we need to check all elements to determine the common type
		LogicalType common_type = LogicalType::VARCHAR;
		bool first_element = true;

		for (size_t idx = 0; idx < node.size(); idx++) {
			LogicalType element_type = DetectYAMLType(node[idx]);

			if (first_element) {
				common_type = element_type;
				first_element = false;
			} else if (common_type.id() == element_type.id()) {
				// If both are structs, merge their fields to handle optional fields
				if (common_type.id() == LogicalTypeId::STRUCT) {
					common_type = MergeStructTypes(common_type, element_type);
				}
				// If both are lists, recursively merge the child types
				else if (common_type.id() == LogicalTypeId::LIST) {
					auto common_child = ListType::GetChildType(common_type);
					auto element_child = ListType::GetChildType(element_type);
					if (common_child.id() == LogicalTypeId::STRUCT && element_child.id() == LogicalTypeId::STRUCT) {
						common_type = LogicalType::LIST(MergeStructTypes(common_child, element_child));
					}
				}
				continue; // Keep checking
			} else if (common_type.IsNumeric() && element_type.IsNumeric()) {
				// Combine numeric types - promote to the wider type
				// First check if either is DOUBLE
				if (common_type.id() == LogicalTypeId::DOUBLE || element_type.id() == LogicalTypeId::DOUBLE) {
					common_type = LogicalType::DOUBLE;
				}
				// Then check integer types in order of size
				else if (common_type.id() == LogicalTypeId::BIGINT || element_type.id() == LogicalTypeId::BIGINT) {
					common_type = LogicalType::BIGINT;
				} else if (common_type.id() == LogicalTypeId::INTEGER || element_type.id() == LogicalTypeId::INTEGER) {
					common_type = LogicalType::INTEGER;
				} else if (common_type.id() == LogicalTypeId::SMALLINT ||
				           element_type.id() == LogicalTypeId::SMALLINT) {
					common_type = LogicalType::SMALLINT;
				}
				// If both are TINYINT, keep TINYINT
			} else {
				common_type = LogicalType::VARCHAR;
				break; // No need to check further once we've fallen back to VARCHAR
			}
		}

		return LogicalType::LIST(common_type);
	}
	case YAML::NodeType::Map: {
		child_list_t<LogicalType> struct_children;
		for (auto it = node.begin(); it != node.end(); ++it) {
			std::string key = it->first.Scalar();
			LogicalType value_type = DetectYAMLType(it->second);
			struct_children.push_back(make_pair(key, value_type));
		}
		return LogicalType::STRUCT(struct_children);
	}
	default:
		return LogicalType::VARCHAR;
	}
}

// Helper function to detect YAML type across multiple documents with jagged schema support
// Uses MergeStructTypes to recursively merge nested struct fields from all documents
LogicalType YAMLReader::DetectJaggedYAMLType(const vector<YAML::Node> &nodes) {
	if (nodes.empty()) {
		return LogicalType::VARCHAR;
	}

	// Use first node as base type
	LogicalType merged_type = DetectYAMLType(nodes[0]);

	// Merge types from all subsequent documents
	for (size_t i = 1; i < nodes.size(); i++) {
		LogicalType node_type = DetectYAMLType(nodes[i]);

		// If both are structs, merge them recursively
		if (merged_type.id() == LogicalTypeId::STRUCT && node_type.id() == LogicalTypeId::STRUCT) {
			merged_type = MergeStructTypes(merged_type, node_type);
		} else if (merged_type.id() != node_type.id()) {
			// Different types, fall back to VARCHAR
			merged_type = LogicalType::VARCHAR;
		}
	}

	return merged_type;
}

// Helper function to convert YAML node to DuckDB value
Value YAMLReader::YAMLNodeToValue(const YAML::Node &node, const LogicalType &target_type) {
	if (!node) {
		return Value(target_type); // NULL value
	}

	// Handle JSON type conversion - applies to all node types
	if ((target_type.HasAlias() && target_type.GetAlias() == "json") || target_type.ToString() == "JSON") {
		// First convert YAML node to YAML string, then use the same path as yaml_to_json function
		YAML::Emitter out;
		yaml_utils::ConfigureEmitter(out, yaml_utils::YAMLFormat::BLOCK);
		out << node;
		std::string yaml_str = out.c_str();

		// Now parse and convert to JSON using the same logic as yaml_to_json
		try {
			const auto docs = yaml_utils::ParseYAML(yaml_str, true);
			std::string json_str;
			if (docs.empty()) {
				json_str = "null";
			} else if (docs.size() == 1) {
				json_str = yaml_utils::YAMLNodeToJSON(docs[0]);
			} else {
				json_str = "[";
				for (idx_t doc_idx = 0; doc_idx < docs.size(); doc_idx++) {
					if (doc_idx > 0) {
						json_str += ",";
					}
					json_str += yaml_utils::YAMLNodeToJSON(docs[doc_idx]);
				}
				json_str += "]";
			}
			return Value(json_str);
		} catch (const std::exception &e) {
			return Value(target_type); // Return NULL on error
		}
	}

	// Handle YAML type conversion - applies to all node types
	if (target_type.HasAlias() && target_type.GetAlias() == "yaml") {
		// Emit as YAML string
		YAML::Emitter out;
		yaml_utils::ConfigureEmitter(out, yaml_utils::YAMLFormat::FLOW);
		out << node;
		return Value(out.c_str());
	}

	// Handle based on YAML node type
	switch (node.Type()) {
	case YAML::NodeType::Scalar: {
		std::string scalar_value = node.Scalar();

		if (target_type.id() == LogicalTypeId::VARCHAR) {
			return Value(scalar_value);
		} else if (target_type.id() == LogicalTypeId::BOOLEAN) {
			// Handle case-insensitive boolean values
			std::string lower_value = scalar_value;
			std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);

			if (lower_value == "true" || lower_value == "yes" || lower_value == "on" || lower_value == "y" ||
			    lower_value == "t") {
				return Value::BOOLEAN(true);
			} else if (lower_value == "false" || lower_value == "no" || lower_value == "off" || lower_value == "n" ||
			           lower_value == "f") {
				return Value::BOOLEAN(false);
			}
			return Value(target_type); // NULL if not valid boolean
		} else if (target_type.id() == LogicalTypeId::TINYINT) {
			try {
				int64_t val = std::stoll(scalar_value);
				if (val >= -128 && val <= 127) {
					return Value::TINYINT(static_cast<int8_t>(val));
				}
				return Value(target_type); // NULL if out of range
			} catch (...) {
				return Value(target_type); // NULL if conversion fails
			}
		} else if (target_type.id() == LogicalTypeId::SMALLINT) {
			try {
				int64_t val = std::stoll(scalar_value);
				if (val >= -32768 && val <= 32767) {
					return Value::SMALLINT(static_cast<int16_t>(val));
				}
				return Value(target_type); // NULL if out of range
			} catch (...) {
				return Value(target_type); // NULL if conversion fails
			}
		} else if (target_type.id() == LogicalTypeId::INTEGER) {
			try {
				int64_t val = std::stoll(scalar_value);
				if (val >= -2147483648LL && val <= 2147483647LL) {
					return Value::INTEGER(static_cast<int32_t>(val));
				}
				return Value(target_type); // NULL if out of range
			} catch (...) {
				return Value(target_type); // NULL if conversion fails
			}
		} else if (target_type.id() == LogicalTypeId::BIGINT) {
			try {
				return Value::BIGINT(std::stoll(scalar_value));
			} catch (...) {
				return Value(target_type); // NULL if conversion fails
			}
		} else if (target_type.id() == LogicalTypeId::DOUBLE) {
			// Handle special floating point values
			std::string lower_val = scalar_value;
			std::transform(lower_val.begin(), lower_val.end(), lower_val.begin(), ::tolower);

			if (lower_val == "inf" || lower_val == "infinity") {
				return Value::DOUBLE(std::numeric_limits<double>::infinity());
			} else if (lower_val == "-inf" || lower_val == "-infinity") {
				return Value::DOUBLE(-std::numeric_limits<double>::infinity());
			} else if (lower_val == "nan") {
				return Value::DOUBLE(std::numeric_limits<double>::quiet_NaN());
			}

			try {
				double val = std::stod(scalar_value);
				return Value::DOUBLE(val);
			} catch (...) {
				return Value(target_type); // NULL if conversion fails
			}
		} else if (target_type.id() == LogicalTypeId::DATE) {
			idx_t pos = 0;
			date_t date_result;
			bool special = false;
			auto date_cast_result =
			    Date::TryConvertDate(scalar_value.c_str(), scalar_value.length(), pos, date_result, special, false);
			if (date_cast_result == DateCastResult::SUCCESS && pos == scalar_value.length()) {
				return Value::DATE(date_result);
			}
			return Value(target_type); // NULL if conversion fails
		} else if (target_type.id() == LogicalTypeId::TIMESTAMP) {
			timestamp_t timestamp_result;
			if (Timestamp::TryConvertTimestamp(scalar_value.c_str(), scalar_value.length(), timestamp_result, false) ==
			    TimestampCastResult::SUCCESS) {
				return Value::TIMESTAMP(timestamp_result);
			}
			return Value(target_type); // NULL if conversion fails
		} else if (target_type.id() == LogicalTypeId::TIME) {
			idx_t pos = 0;
			dtime_t time_result;
			if (Time::TryConvertTime(scalar_value.c_str(), scalar_value.length(), pos, time_result, false) &&
			    pos == scalar_value.length()) {
				return Value::TIME(time_result);
			}
			return Value(target_type); // NULL if conversion fails
		}
		return Value(scalar_value); // Default to string
	}
	case YAML::NodeType::Sequence: {
		if (target_type.id() != LogicalTypeId::LIST) {
			return Value(target_type); // NULL if not expecting a list
		} else if (node.size() == 0) {
			// Empty lists need an explicit type (ISSUE #8)
			return Value::LIST(target_type, {});
		}

		// Get child type for list
		auto child_type = ListType::GetChildType(target_type);

		// Create list of values - recursively convert each element
		vector<Value> values;
		for (size_t idx = 0; idx < node.size(); idx++) {
			values.push_back(YAMLNodeToValue(node[idx], child_type));
		}

		return Value::LIST(values);
	}
	case YAML::NodeType::Map: {
		if (target_type.id() != LogicalTypeId::STRUCT) {
			return Value(target_type); // NULL if not expecting a struct
		}

		// Get struct children
		auto &struct_children = StructType::GetChildTypes(target_type);

		// Create struct values
		child_list_t<Value> struct_values;
		for (auto &entry : struct_children) {
			if (node[entry.first]) {
				struct_values.push_back(make_pair(entry.first, YAMLNodeToValue(node[entry.first], entry.second)));
			} else {
				struct_values.push_back(make_pair(entry.first, Value(entry.second))); // NULL value
			}
		}

		return Value::STRUCT(struct_values);
	}
	default:
		return Value(target_type); // NULL for unknown type
	}
}

} // namespace duckdb
