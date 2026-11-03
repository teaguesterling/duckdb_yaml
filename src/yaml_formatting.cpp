#include "yaml_formatting.hpp"
#include "duckdb/common/types/value.hpp"

namespace duckdb {

namespace yaml_formatting {

//===--------------------------------------------------------------------===//
// Layout Transformation Functions
//===--------------------------------------------------------------------===//

std::string ApplySequenceLayout(const std::string &yaml_str) {
	if (yaml_str.empty()) {
		return yaml_str;
	}

	std::string result = "- " + yaml_str;

	// Add 2 spaces of indentation to continuation lines
	size_t pos = result.find('\n');
	while (pos != std::string::npos && pos + 1 < result.length()) {
		result.insert(pos + 1, "  ");
		pos = result.find('\n', pos + 3); // Skip the inserted spaces
	}

	return result;
}

std::string FormatValueWithLayout(const Value &value, yaml_utils::YAMLFormat format, YAMLLayout layout,
                                  yaml_utils::YAMLStringStyle string_style, idx_t indent) {
	// Get the base YAML string
	std::string yaml_str = yaml_utils::ValueToYAMLString(value, format, string_style, indent);

	if (layout == YAMLLayout::SEQUENCE) {
		return ApplySequenceLayout(yaml_str);
	}

	// For DOCUMENT layout, return as-is (document separators are handled separately)
	return yaml_str;
}

std::string PostProcessForLayout(const std::string &yaml_str, YAMLLayout layout) {
	if (layout == YAMLLayout::SEQUENCE) {
		return ApplySequenceLayout(yaml_str);
	}

	// A DOCUMENT is written as-is. The COPY writer adds separators, because only it knows which row starts a file.
	return yaml_str;
}

} // namespace yaml_formatting

} // namespace duckdb
