#include "yaml_formatting.hpp"
#include "duckdb/common/types/value.hpp"

namespace duckdb {

namespace yaml_formatting {

//===--------------------------------------------------------------------===//
// Layout Transformation Functions
//===--------------------------------------------------------------------===//

std::string ApplySequenceLayout(const std::string& yaml_str) {
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

std::string ApplyDocumentSeparator(const std::string& yaml_str, bool is_first_document, bool is_block_style) {
    if (!is_block_style || is_first_document) {
        return yaml_str;
    }
    
    return "---\n" + yaml_str;
}

std::string FormatValueWithLayout(const Value& value, yaml_utils::YAMLFormat format, YAMLLayout layout) {
    // Get the base YAML string
    std::string yaml_str = yaml_utils::ValueToYAMLString(value, format);
    
    if (layout == YAMLLayout::SEQUENCE) {
        return ApplySequenceLayout(yaml_str);
    }
    
    // For DOCUMENT layout, return as-is (document separators are handled separately)
    return yaml_str;
}

std::string PostProcessForLayout(const std::string& yaml_str, YAMLLayout layout, yaml_utils::YAMLFormat format, idx_t row_index) {
    if (layout == YAMLLayout::SEQUENCE) {
        return ApplySequenceLayout(yaml_str);
    } else if (layout == YAMLLayout::DOCUMENT) {
        bool is_first_document = (row_index == 0);
        bool is_block_style = (format == yaml_utils::YAMLFormat::BLOCK);
        return ApplyDocumentSeparator(yaml_str, is_first_document, is_block_style);
    }
    
    // Fallback - return as-is
    return yaml_str;
}

} // namespace yaml_formatting

} // namespace duckdb