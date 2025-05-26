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
    FLOW,  // Inline format
    BLOCK  // Multi-line format
};

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
void ConfigureEmitter(YAML::Emitter& out, YAMLFormat format);

// Emit single YAML document
std::string EmitYAML(const YAML::Node& node, YAMLFormat format);

// Emit multiple YAML documents
std::string EmitYAMLMultiDoc(const std::vector<YAML::Node>& docs, YAMLFormat format);

// Parse YAML string (supports multi-document)
std::vector<YAML::Node> ParseYAML(const std::string& yaml_str, bool multi_document = true);

//===--------------------------------------------------------------------===//
// YAML to JSON Conversion
//===--------------------------------------------------------------------===//

// Convert YAML node to JSON string
std::string YAMLNodeToJSON(const YAML::Node& node);

//===--------------------------------------------------------------------===//
// DuckDB Value to YAML Conversion
//===--------------------------------------------------------------------===//

// Convert DuckDB Value to YAML::Node
YAML::Node ValueToYAMLNode(const Value& value);

// Emit DuckDB Value to YAML::Emitter
void EmitValueToYAML(YAML::Emitter& out, const Value& value);

// Convert DuckDB Value to YAML string
std::string ValueToYAMLString(const Value& value, YAMLFormat format);

// Format with style and layout logic (simplified - no layout handling)
std::string FormatPerStyleAndLayout(const Value& value, YAMLFormat format, const std::string& layout);

} // namespace yaml_utils

} // namespace duckdb