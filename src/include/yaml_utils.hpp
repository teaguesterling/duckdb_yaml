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

//===--------------------------------------------------------------------===//
// Resource limits (DoS hardening — GHSA-h5hw-g5m6-vmjj)
//===--------------------------------------------------------------------===//
// Default upper bounds applied while traversing/materializing YAML.
//
// - MAX_EXPANSION_NODES bounds the *total* number of node visits during a
//   single top-level conversion. Because alias/anchor references are resolved
//   to shared nodes (a DAG) but every consumer re-materializes them, a small
//   "&a […] *a …" chain expands exponentially; counting every visit (without
//   de-duplication) bounds that expansion. The default is well above the node
//   count of any legitimate document that fits under MAX_INPUT_SIZE (a 16 MB
//   input holds at most a few million nodes), so real files are unaffected.
// - MAX_NESTING_DEPTH bounds our own recursive descent. yaml-cpp already caps
//   *parse* depth at 500 (DepthGuard) and throws a catchable exception, so this
//   is a defense-in-depth guard that mainly covers the Value→YAML paths, which
//   do not pass through the parser.
// - MAX_INPUT_SIZE bounds string input to the scalar functions and the
//   frontmatter reader, which previously had no size cap (only the file
//   readers did).
constexpr idx_t YAML_DEFAULT_MAX_EXPANSION_NODES = 50000000; // 50M node visits
constexpr idx_t YAML_DEFAULT_MAX_NESTING_DEPTH = 1000;       // recursion depth
constexpr idx_t YAML_DEFAULT_MAX_INPUT_SIZE = 16777216;      // 16 MB

// Global YAML settings management
class YAMLSettings {
public:
	static void SetDefaultFormat(YAMLFormat format);
	static YAMLFormat GetDefaultFormat();

	// Resource limits (see constants above). Runtime-configurable so operators
	// can raise/lower them; exposed to SQL via yaml_set_*/yaml_get_* functions.
	static void SetMaxExpansionNodes(idx_t value);
	static idx_t GetMaxExpansionNodes();
	static void SetMaxNestingDepth(idx_t value);
	static idx_t GetMaxNestingDepth();
	static void SetMaxInputSize(idx_t value);
	static idx_t GetMaxInputSize();

private:
	static YAMLFormat default_format;
	static idx_t max_expansion_nodes;
	static idx_t max_nesting_depth;
	static idx_t max_input_size;
};

//===--------------------------------------------------------------------===//
// Traversal budget (bounds recursion depth and total node expansion)
//===--------------------------------------------------------------------===//
// Shared counter threaded through the recursive YAML traversals. A single
// instance is created at each top-level entry point and passed by reference so
// that the cost of re-materializing shared alias nodes accumulates across the
// whole conversion.
struct YAMLTraversalBudget {
	idx_t nodes = 0;
	idx_t depth = 0;
	idx_t max_nodes;
	idx_t max_depth;

	YAMLTraversalBudget()
	    : max_nodes(YAMLSettings::GetMaxExpansionNodes()), max_depth(YAMLSettings::GetMaxNestingDepth()) {
	}
	YAMLTraversalBudget(idx_t max_nodes_p, idx_t max_depth_p) : max_nodes(max_nodes_p), max_depth(max_depth_p) {
	}
};

// RAII helper: on construction it counts one node visit and increments the
// current depth, throwing an InvalidInputException if either limit is exceeded;
// on destruction it restores the depth. The node count is cumulative (never
// decremented) so repeated expansion of shared alias nodes is bounded.
struct YAMLBudgetScope {
	explicit YAMLBudgetScope(YAMLTraversalBudget &budget);
	~YAMLBudgetScope();
	YAMLBudgetScope(const YAMLBudgetScope &) = delete;
	YAMLBudgetScope &operator=(const YAMLBudgetScope &) = delete;

private:
	YAMLTraversalBudget &budget;
};

// Validate that a string input does not exceed the configured maximum size.
// Throws InvalidInputException when the limit is exceeded. `context` is used
// only for the error message (which function rejected the input).
void CheckInputSize(idx_t size, const char *context);

// Walk a parsed node, counting every visit (re-materializing shared alias
// nodes without de-duplication) and throwing if the configured expansion or
// depth budget is exceeded. Use at the entry of functions that traverse the
// whole node tree with their own recursion (e.g. structure/contains/merge)
// so an alias/anchor bomb is rejected before the expensive work runs.
void CheckExpansionBudget(const YAML::Node &node);

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
