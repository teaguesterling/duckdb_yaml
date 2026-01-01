#pragma once

#include "yaml_utils.hpp"
#include "duckdb.hpp"
#include <string>

namespace duckdb {

namespace yaml_formatting {

//===--------------------------------------------------------------------===//
// Layout Types
//===--------------------------------------------------------------------===//

enum class YAMLLayout {
	DOCUMENT, // Each row as a separate YAML document
	SEQUENCE  // All rows as items in a YAML sequence
};

//===--------------------------------------------------------------------===//
// Layout Transformation Functions
//===--------------------------------------------------------------------===//

// Apply sequence layout formatting to a YAML string
// Prepends "- " and indents continuation lines with 2 spaces
std::string ApplySequenceLayout(const std::string &yaml_str);

// Apply document separator for multi-document YAML
// Prepends "---\n" for block style documents (not for the first document)
std::string ApplyDocumentSeparator(const std::string &yaml_str, bool is_first_document, bool is_block_style);

// Format a single value with the specified layout
std::string FormatValueWithLayout(const Value &value, yaml_utils::YAMLFormat format, YAMLLayout layout);

// Post-process YAML output for multiple rows based on layout
std::string PostProcessForLayout(const std::string &yaml_str, YAMLLayout layout, yaml_utils::YAMLFormat format,
                                 idx_t row_index);

} // namespace yaml_formatting

} // namespace duckdb
