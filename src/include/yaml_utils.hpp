#pragma once

#include "duckdb.hpp"
#include "yaml-cpp/yaml.h"
#include <string>
#include <vector>

namespace duckdb {

namespace yaml_utils {

//===--------------------------------------------------------------------===//
// YAML Format Settings
//===--------------------------------------------------------------------===//

enum class YAMLFormat {
	FLOW, // Inline format
	BLOCK // Multi-line format
};

enum class YAMLStringStyle {
	AUTO,    // Resolves to LITERAL for block, QUOTED for flow
	LITERAL, // Use YAML literal block scalar (|) for multiline strings
	QUOTED   // Use quoted strings with escape sequences
};

// Resolve AUTO to a concrete style based on format
YAMLStringStyle ResolveStringStyle(YAMLStringStyle style, YAMLFormat format);

// Global YAML settings management
class YAMLSettings {
public:
	static void SetDefaultFormat(YAMLFormat format);
	static YAMLFormat GetDefaultFormat();

private:
	static YAMLFormat default_format;
};

//===--------------------------------------------------------------------===//
// YAML Parsing and Emission
//===--------------------------------------------------------------------===//

// Configure YAML emitter with format settings
void ConfigureEmitter(YAML::Emitter &out, YAMLFormat format, idx_t indent = 2);

// Emit a YAML::Node tree, applying YAML::Literal to multiline scalars when style is LITERAL
void EmitNodeWithStringStyle(YAML::Emitter &out, const YAML::Node &node, YAMLStringStyle resolved_style);

// Emit single YAML document
std::string EmitYAML(const YAML::Node &node, YAMLFormat format, YAMLStringStyle string_style = YAMLStringStyle::AUTO,
                     idx_t indent = 2);

// Emit multiple YAML documents
std::string EmitYAMLMultiDoc(const std::vector<YAML::Node> &docs, YAMLFormat format);

// Parse YAML string (supports multi-document)
std::vector<YAML::Node> ParseYAML(const std::string &yaml_str, bool multi_document = true);

//===--------------------------------------------------------------------===//
// YAML to JSON Conversion
//===--------------------------------------------------------------------===//

// Convert YAML node to JSON string
std::string YAMLNodeToJSON(const YAML::Node &node);

//===--------------------------------------------------------------------===//
// DuckDB Value to YAML Conversion
//===--------------------------------------------------------------------===//

// Convert DuckDB Value to YAML::Node
YAML::Node ValueToYAMLNode(const Value &value);

// Emit DuckDB Value to YAML::Emitter
void EmitValueToYAML(YAML::Emitter &out, const Value &value);

// Convert DuckDB Value to YAML string
std::string ValueToYAMLString(const Value &value, YAMLFormat format,
                              YAMLStringStyle string_style = YAMLStringStyle::AUTO, idx_t indent = 2);

// Format with style and layout logic (simplified - no layout handling)
std::string FormatPerStyleAndLayout(const Value &value, YAMLFormat format, const std::string &layout);

} // namespace yaml_utils

} // namespace duckdb
