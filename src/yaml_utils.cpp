#include "yaml_utils.hpp"
#include "yaml_reader.hpp"
#include "duckdb_compat.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/types/date.hpp"
#include "duckdb/common/types/timestamp.hpp"
#include "duckdb/common/types/time.hpp"
#include "duckdb/common/operator/cast_operators.hpp"
#include "duckdb/function/cast/default_casts.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace duckdb {

namespace yaml_utils {

//===--------------------------------------------------------------------===//
// YAML Settings Implementation
//===--------------------------------------------------------------------===//

// Initialize default format to FLOW for better compatibility with SQLLogicTest
YAMLFormat YAMLSettings::default_format = YAMLFormat::FLOW;

// Resource-limit defaults (see yaml_utils.hpp / GHSA-h5hw-g5m6-vmjj)
idx_t YAMLSettings::max_expansion_nodes = YAML_DEFAULT_MAX_EXPANSION_NODES;
idx_t YAMLSettings::max_nesting_depth = YAML_DEFAULT_MAX_NESTING_DEPTH;
idx_t YAMLSettings::max_input_size = YAML_DEFAULT_MAX_INPUT_SIZE;

YAMLFormat YAMLSettings::GetDefaultFormat() {
	return default_format;
}

void YAMLSettings::SetDefaultFormat(YAMLFormat format) {
	default_format = format;
}

void YAMLSettings::SetMaxExpansionNodes(idx_t value) {
	max_expansion_nodes = value;
}
idx_t YAMLSettings::GetMaxExpansionNodes() {
	return max_expansion_nodes;
}
void YAMLSettings::SetMaxNestingDepth(idx_t value) {
	max_nesting_depth = value;
}
idx_t YAMLSettings::GetMaxNestingDepth() {
	return max_nesting_depth;
}
void YAMLSettings::SetMaxInputSize(idx_t value) {
	max_input_size = value;
}
idx_t YAMLSettings::GetMaxInputSize() {
	return max_input_size;
}

//===--------------------------------------------------------------------===//
// Traversal budget / input-size guard (GHSA-h5hw-g5m6-vmjj)
//===--------------------------------------------------------------------===//

YAMLBudgetScope::YAMLBudgetScope(YAMLTraversalBudget &budget_p) : budget(budget_p) {
	// Count this node visit first. The counter is cumulative and is never
	// decremented, so exponential re-materialization of shared alias nodes is
	// bounded regardless of depth.
	if (++budget.nodes > budget.max_nodes) {
		throw InvalidInputException(
		    "YAML expansion exceeded the maximum node budget (%llu); the input may contain an alias/anchor "
		    "expansion bomb. Raise the limit with yaml_set_max_expansion_nodes(N) if this is a legitimate document.",
		    (unsigned long long)budget.max_nodes);
	}
	if (++budget.depth > budget.max_depth) {
		throw InvalidInputException(
		    "YAML nesting exceeded the maximum depth (%llu). Raise the limit with yaml_set_max_nesting_depth(N) if "
		    "this is a legitimate document.",
		    (unsigned long long)budget.max_depth);
	}
}

YAMLBudgetScope::~YAMLBudgetScope() {
	--budget.depth;
}

void CheckInputSize(idx_t size, const char *context) {
	idx_t limit = YAMLSettings::GetMaxInputSize();
	if (size > limit) {
		throw InvalidInputException("YAML input to %s (%llu bytes) exceeds the maximum allowed size (%llu bytes). "
		                            "Raise the limit with yaml_set_max_input_size(N).",
		                            context, (unsigned long long)size, (unsigned long long)limit);
	}
}

static void CheckExpansionBudgetImpl(const YAML::Node &node, YAMLTraversalBudget &budget) {
	if (!node) {
		return;
	}
	YAMLBudgetScope scope(budget);
	switch (node.Type()) {
	case YAML::NodeType::Sequence:
		for (idx_t i = 0; i < node.size(); i++) {
			CheckExpansionBudgetImpl(node[i], budget);
		}
		break;
	case YAML::NodeType::Map:
		for (auto it = node.begin(); it != node.end(); ++it) {
			CheckExpansionBudgetImpl(it->second, budget);
		}
		break;
	default:
		break;
	}
}

void CheckExpansionBudget(const YAML::Node &node) {
	YAMLTraversalBudget budget;
	CheckExpansionBudgetImpl(node, budget);
}

//===--------------------------------------------------------------------===//
// YAML Parsing and Emission
//===--------------------------------------------------------------------===//

std::vector<YAML::Node> ParseYAML(const std::string &yaml_str, bool multi_doc) {
	if (yaml_str.empty()) {
		return {};
	}

	try {
		std::stringstream yaml_stream(yaml_str);
		if (multi_doc) {
			return YAML::LoadAll(yaml_stream);
		} else {
			std::vector<YAML::Node> result;
			result.push_back(YAML::Load(yaml_stream));
			return result;
		}
	} catch (const std::exception &e) {
		throw InvalidInputException("Error parsing YAML: %s", e.what());
	}
}

YAMLStringStyle ResolveStringStyle(YAMLStringStyle style, YAMLFormat format) {
	// A flow collection cannot hold a block scalar, so flow is always quoted.
	if (format == YAMLFormat::FLOW) {
		return YAMLStringStyle::QUOTED;
	}
	if (style != YAMLStringStyle::AUTO) {
		return style;
	}
	return (format == YAMLFormat::BLOCK) ? YAMLStringStyle::LITERAL : YAMLStringStyle::QUOTED;
}

void ConfigureEmitter(YAML::Emitter &out, YAMLFormat format, idx_t indent) {
	out.SetIndent(indent);
	if (format == YAMLFormat::FLOW) {
		out.SetMapFormat(YAML::Flow);
		out.SetSeqFormat(YAML::Flow);
	} else {
		out.SetMapFormat(YAML::Block);
		out.SetSeqFormat(YAML::Block);
	}
}

bool IsStringScalar(const YAML::Node &node) {
	// yaml-cpp's parser tags every non-plain (quoted or block) scalar "!" and plain ones "?".
	return node.IsScalar() && (node.Tag() == "!" || node.Tag() == "tag:yaml.org,2002:str");
}

static bool AllCharsIn(const std::string &value, size_t start, const char *chars) {
	return start < value.size() && value.find_first_not_of(chars, start) == std::string::npos;
}

bool ParseCoreInteger(const std::string &value, int64_t &result) {
	int base;
	size_t digits_start = 0;
	if (value.size() > 2 && value[0] == '0' && value[1] == 'x') {
		base = 16;
		digits_start = 2;
		if (!AllCharsIn(value, digits_start, "0123456789abcdefABCDEF")) {
			return false;
		}
	} else if (value.size() > 2 && value[0] == '0' && value[1] == 'o') {
		base = 8;
		digits_start = 2;
		if (!AllCharsIn(value, digits_start, "01234567")) {
			return false;
		}
	} else {
		base = 10;
		const size_t sign = !value.empty() && (value[0] == '-' || value[0] == '+') ? 1 : 0;
		if (!AllCharsIn(value, sign, "0123456789")) {
			return false;
		}
	}
	errno = 0;
	result = std::strtoll(value.c_str() + digits_start, nullptr, base);
	return errno != ERANGE;
}

bool ParseCoreFloat(const std::string &value, double &result) {
	if (value == ".nan" || value == ".NaN" || value == ".NAN") {
		result = std::numeric_limits<double>::quiet_NaN();
		return true;
	}
	size_t pos = !value.empty() && (value[0] == '-' || value[0] == '+') ? 1 : 0;
	const std::string unsigned_value = value.substr(pos);
	if (unsigned_value == ".inf" || unsigned_value == ".Inf" || unsigned_value == ".INF") {
		result = value[0] == '-' ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
		return true;
	}
	// [-+]?(\.[0-9]+|[0-9]+(\.[0-9]*)?)([eE][-+]?[0-9]+)?, with a fraction or an exponent. strtod alone is wider: it
	// also reads hexadecimal floats and the words inf and nan.
	const auto skip_digits = [&]() {
		const size_t start = pos;
		while (pos < value.size() && value[pos] >= '0' && value[pos] <= '9') {
			pos++;
		}
		return pos - start;
	};
	const size_t int_digits = skip_digits();
	size_t fraction_digits = 0;
	bool has_point = false;
	if (pos < value.size() && value[pos] == '.') {
		has_point = true;
		pos++;
		fraction_digits = skip_digits();
	}
	if (int_digits == 0 && fraction_digits == 0) {
		return false;
	}
	bool has_exponent = false;
	if (pos < value.size() && (value[pos] == 'e' || value[pos] == 'E')) {
		has_exponent = true;
		pos++;
		if (pos < value.size() && (value[pos] == '-' || value[pos] == '+')) {
			pos++;
		}
		if (skip_digits() == 0) {
			return false;
		}
	}
	if (pos != value.size() || (!has_point && !has_exponent)) {
		return false;
	}
	errno = 0;
	result = std::strtod(value.c_str(), nullptr);
	return errno != ERANGE;
}

YAMLCoreKind ClassifyPlainScalar(const std::string &value) {
	if (value.empty() || value == "~" || value == "null" || value == "Null" || value == "NULL") {
		return YAMLCoreKind::NULL_VALUE;
	}
	if (value == "true" || value == "True" || value == "TRUE" || value == "false" || value == "False" ||
	    value == "FALSE") {
		return YAMLCoreKind::BOOLEAN;
	}
	int64_t int_value;
	if (ParseCoreInteger(value, int_value)) {
		return YAMLCoreKind::INTEGER;
	}
	double float_value;
	if (ParseCoreFloat(value, float_value)) {
		return YAMLCoreKind::FLOAT;
	}
	return YAMLCoreKind::STRING;
}

YAMLTemporal DetectTemporal(const std::string &value) {
	YAMLTemporal result;
	// DuckDB also parses epoch and infinity as dates. In YAML these are words.
	if (value.empty() || !StringUtil::CharacterIsDigit(value[0])) {
		return result;
	}
	idx_t pos = 0;
	// Only YYYY-: DuckDB reads 26-11-03 as the year 26, and read_json has no format with slashes
	if (value.find('-') == 4) {
		bool special = false;
		if (Date::TryConvertDate(value.c_str(), value.size(), pos, result.date, special, false) ==
		        DateCastResult::SUCCESS &&
		    pos == value.size()) {
			result.kind = YAMLTemporalKind::DATE;
			return result;
		}

		// A space before an offset parses as a time zone name, and a time zone name cannot be applied here, so it
		// stays text. read_json has no timestamp format without seconds, unless it has an offset.
		bool has_offset = false;
		string_t time_zone(nullptr, 0);
		if (Timestamp::TryConvertTimestampTZ(value.c_str(), value.size(), result.timestamp, true, has_offset,
		                                     time_zone) == TimestampCastResult::SUCCESS &&
		    time_zone.GetSize() == 0) {
			const auto first_colon = value.find(':');
			if (has_offset) {
				result.kind = YAMLTemporalKind::TIMESTAMP_TZ;
				return result;
			} else if (first_colon != std::string::npos && value.find(':', first_colon + 1) != std::string::npos) {
				result.kind = YAMLTemporalKind::TIMESTAMP;
				return result;
			}
		}
		return result;
	}

	pos = 0;
	if (Time::TryConvertTime(value.c_str(), value.size(), pos, result.time, false) && pos == value.size()) {
		result.kind = YAMLTemporalKind::TIME;
	}
	return result;
}

// A string that is a string under the 1.2 core schema, but that a YAML 1.1 parser or a looser number parser reads as
// something else. Most YAML parsers in use follow 1.1, so these stay quoted.
static bool IsAmbiguousAcrossYAMLVersions(const std::string &value) {
	std::string lower_value = value;
	std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
	if (lower_value == "y" || lower_value == "n" || lower_value == "yes" || lower_value == "no" ||
	    lower_value == "on" || lower_value == "off" || lower_value == "true" || lower_value == "false" ||
	    lower_value == "null") {
		return true;
	}
	if (value.empty() ||
	    (!StringUtil::CharacterIsDigit(value[0]) && value[0] != '-' && value[0] != '+' && value[0] != '.')) {
		return false;
	}
	if (StringUtil::CharacterIsDigit(value[0])) {
		// Not detected here, but a timestamp or a date to a YAML 1.1 parser or to DuckDB's casts
		idx_t pos = 0;
		date_t date_value;
		bool special = false;
		timestamp_t timestamp_value;
		if ((Date::TryConvertDate(value.c_str(), value.size(), pos, date_value, special, false) ==
		         DateCastResult::SUCCESS &&
		     pos == value.size()) ||
		    Timestamp::TryConvertTimestamp(value.c_str(), value.size(), timestamp_value, false) ==
		        TimestampCastResult::SUCCESS) {
			return true;
		}
	}
	if (value.find_first_not_of("-+0123456789_") == std::string::npos ||
	    (value.size() > 2 && lower_value.compare(0, 2, "0b") == 0)) {
		return true; // 1_000 and 0b101 are integers in YAML 1.1
	}
	char *parse_end;
	std::strtod(value.c_str(), &parse_end);
	return parse_end == value.c_str() + value.size(); // 0X1F, and .InF on the parsers that fold case
}

// Whether writing this string scalar plain could read back as something else: here, or in another parser. yaml-cpp's
// emitter writes any string plain when it can, so "123" would come back as the number 123.
static bool ChangesWhenPlain(const YAML::Node &node) {
	const auto &value = node.Scalar();
	return ClassifyPlainScalar(value) != YAMLCoreKind::STRING || IsAmbiguousAcrossYAMLVersions(value) ||
	       YAMLReader::DetectYAMLType(YAML::Node(value)) != LogicalType::VARCHAR;
}

// yaml-cpp writes every literal block as a bare `|`, which means "exactly one trailing newline". It has no way to
// write `|-`. So a string with no trailing newline is emitted with this marker (U+10FFFD, a private-use character) in
// front of it, and ApplyLiteralStripMarkers then turns `|`, newline, indent, marker into `|-`, newline, indent.
static const std::string LITERAL_STRIP_MARKER = "\xF4\x8F\xBF\xBD";

// Whether a literal block, with `|` or `|-`, reads back as exactly this string. A block cannot start with whitespace
// without an indentation indicator, keep several trailing newlines without `|+`, or hold other control characters.
static bool FitsLiteralBlock(const std::string &value) {
	const char first = value[0];
	if (first == ' ' || first == '\t' || first == '\n') {
		return false;
	}
	const auto last_content = value.find_last_not_of(" \t");
	if (last_content == std::string::npos || (value[last_content] == '\n' && last_content + 1 != value.size())) {
		return false; // The last line is only spaces or tabs, which a block scalar does not keep.
	}
	if (value.size() >= 2 && value[value.size() - 1] == '\n' && value[value.size() - 2] == '\n') {
		return false;
	}
	for (const char ch : value) {
		if (static_cast<unsigned char>(ch) < 0x20 && ch != '\n' && ch != '\t') {
			return false;
		}
	}
	return value.find(LITERAL_STRIP_MARKER) == std::string::npos;
}

static std::string ApplyLiteralStripMarkers(std::string yaml) {
	auto marker = yaml.find(LITERAL_STRIP_MARKER);
	while (marker != std::string::npos) {
		auto line_start = marker;
		while (line_start > 0 && yaml[line_start - 1] == ' ') {
			line_start--;
		}
		auto next_search = marker + LITERAL_STRIP_MARKER.size();
		if (line_start >= 2 && yaml[line_start - 1] == '\n' && yaml[line_start - 2] == '|') {
			yaml.erase(marker, LITERAL_STRIP_MARKER.size());
			yaml.insert(line_start - 1, 1, '-');
			next_search = marker + 1;
		}
		marker = yaml.find(LITERAL_STRIP_MARKER, next_search);
	}
	return yaml;
}

static void EmitNodeWithStringStyleImpl(YAML::Emitter &out, const YAML::Node &node, YAMLStringStyle resolved_style,
                                        YAMLTraversalBudget &budget) {
	YAMLBudgetScope scope(budget);
	switch (node.Type()) {
	case YAML::NodeType::Scalar: {
		const auto &scalar = node.Scalar();
		// Only apply Literal style to scalars that actually contain newlines
		if (resolved_style == YAMLStringStyle::LITERAL && scalar.find('\n') != std::string::npos) {
			if (!FitsLiteralBlock(scalar)) {
				out << YAML::DoubleQuoted << scalar;
			} else if (scalar.back() == '\n') {
				out << YAML::Literal << scalar;
			} else {
				out << YAML::Literal << (LITERAL_STRIP_MARKER + scalar);
			}
		} else if (IsStringScalar(node) && ChangesWhenPlain(node)) {
			out << YAML::DoubleQuoted << scalar;
		} else {
			out << node;
		}
		break;
	}
	case YAML::NodeType::Sequence: {
		out << YAML::BeginSeq;
		for (const auto &child : node) {
			EmitNodeWithStringStyleImpl(out, child, resolved_style, budget);
		}
		out << YAML::EndSeq;
		break;
	}
	case YAML::NodeType::Map: {
		out << YAML::BeginMap;
		for (const auto &pair : node) {
			out << YAML::Key;
			// Keys always use default emission (never literal)
			out << pair.first;
			out << YAML::Value;
			EmitNodeWithStringStyleImpl(out, pair.second, resolved_style, budget);
		}
		out << YAML::EndMap;
		break;
	}
	default:
		// Null and Undefined - delegate directly
		out << node;
		break;
	}
}

void EmitNodeWithStringStyle(YAML::Emitter &out, const YAML::Node &node, YAMLStringStyle resolved_style) {
	YAMLTraversalBudget budget;
	EmitNodeWithStringStyleImpl(out, node, resolved_style, budget);
}

std::string EmitYAML(const YAML::Node &node, YAMLFormat format, YAMLStringStyle string_style, idx_t indent) {
	if (node.IsNull()) {
		return "~";
	}
	YAML::Emitter out;
	ConfigureEmitter(out, format, indent);
	const auto resolved = ResolveStringStyle(string_style, format);
	EmitNodeWithStringStyle(out, node, resolved);
	return resolved == YAMLStringStyle::LITERAL ? ApplyLiteralStripMarkers(out.c_str()) : std::string(out.c_str());
}

std::string EmitYAMLMultiDoc(const std::vector<YAML::Node> &docs, YAMLFormat format) {
	if (docs.empty()) {
		return "";
	}

	if (docs.size() == 1) {
		return EmitYAML(docs[0], format);
	}

	if (format == YAMLFormat::FLOW) {
		// For inline format, emit as sequence for readability
		YAML::Emitter out;
		ConfigureEmitter(out, format);
		out << YAML::BeginSeq;
		for (const auto &doc : docs) {
			EmitNodeWithStringStyle(out, doc, YAMLStringStyle::QUOTED);
		}
		out << YAML::EndSeq;
		return out.c_str();
	} else {
		// For block format, emit as multiple documents
		std::string result;
		for (idx_t doc_idx = 0; doc_idx < docs.size(); doc_idx++) {
			if (doc_idx > 0) {
				result += "\n---\n";
			}
			result += EmitYAML(docs[doc_idx], format);
		}
		return result;
	}
}

//===--------------------------------------------------------------------===//
// YAML to JSON Conversion
//===--------------------------------------------------------------------===//

// Helper function to check if a string might be a date/timestamp
static bool TryDetectDateOrTimestamp(const std::string &value, std::string &json_value) {
	const auto temporal = DetectTemporal(value);
	switch (temporal.kind) {
	case YAMLTemporalKind::DATE:
		json_value = "\"" + Date::ToString(temporal.date) + "\"";
		return true;
	case YAMLTemporalKind::TIME:
		json_value = "\"" + Time::ToString(temporal.time) + "\"";
		return true;
	case YAMLTemporalKind::TIMESTAMP:
		// No zone in the input, so none in the output
		json_value = "\"" + Timestamp::ToString(temporal.timestamp) + "\"";
		return true;
	case YAMLTemporalKind::TIMESTAMP_TZ:
		json_value = "\"" + Timestamp::ToString(temporal.timestamp) + "Z\"";
		return true;
	case YAMLTemporalKind::NONE:
		break;
	}
	return false;
}

// JSON-escape a string per RFC 8259 and wrap it in quotes: the named short escapes plus
// \u00XX for any control character (< 0x20). Used for BOTH string scalar values and map keys.
// Keys were previously inserted raw, so a key containing " or \ (e.g. `a"b: 1`) produced
// invalid JSON from a function declared to return LogicalType::JSON() (issue #42).
static std::string EscapeJSONString(const std::string &value) {
	std::string result = "\"";
	for (char ch : value) {
		switch (ch) {
		case '\"':
			result += "\\\"";
			break;
		case '\\':
			result += "\\\\";
			break;
		case '\b':
			result += "\\b";
			break;
		case '\f':
			result += "\\f";
			break;
		case '\n':
			result += "\\n";
			break;
		case '\r':
			result += "\\r";
			break;
		case '\t':
			result += "\\t";
			break;
		default: {
			constexpr unsigned char MIN_PRINTABLE_CHAR = 32;
			constexpr idx_t UNICODE_BUFFER_SIZE = 8;
			if (static_cast<unsigned char>(ch) < MIN_PRINTABLE_CHAR) {
				char buf[UNICODE_BUFFER_SIZE];
				snprintf(buf, sizeof(buf), "\\u%04x", ch);
				result += buf;
			} else {
				result += ch;
			}
		}
		}
	}
	result += "\"";
	return result;
}

static std::string YAMLNodeToJSONImpl(const YAML::Node &node, YAMLTraversalBudget &budget) {
	if (!node) {
		return "null";
	}
	YAMLBudgetScope scope(budget);

	switch (node.Type()) {
	case YAML::NodeType::Null:
		return "null";
	case YAML::NodeType::Scalar: {
		const auto value = node.Scalar();
		if (IsStringScalar(node)) {
			// A quoted scalar is a JSON string; dates and times are normalized as below.
			std::string json_value;
			return TryDetectDateOrTimestamp(value, json_value) ? json_value : EscapeJSONString(value);
		}

		switch (ClassifyPlainScalar(value)) {
		case YAMLCoreKind::NULL_VALUE:
			return "null";
		case YAMLCoreKind::BOOLEAN:
			return value[0] == 't' || value[0] == 'T' ? "true" : "false";
		case YAMLCoreKind::INTEGER: {
			// In the decimal form that JSON requires, for 0x1F, 0o17, 007 and +7
			int64_t int_value = 0;
			ParseCoreInteger(value, int_value);
			return std::to_string(int_value);
		}
		case YAMLCoreKind::FLOAT: {
			double float_value = 0;
			ParseCoreFloat(value, float_value);
			if (std::isinf(float_value)) {
				return float_value < 0 ? "\"-Infinity\"" : "\"Infinity\"";
			} else if (std::isnan(float_value)) {
				return "\"NaN\"";
			}
			return Value::DOUBLE(float_value).ToString();
		}
		case YAMLCoreKind::STRING:
			break;
		}

		// Try to detect date/timestamp/time
		std::string json_value;
		if (TryDetectDateOrTimestamp(value, json_value)) {
			return json_value;
		}

		// If all else fails, treat as string and escape JSON special characters.
		return EscapeJSONString(value);
	}
	case YAML::NodeType::Sequence: {
		std::string result = "[";
		for (idx_t seq_idx = 0; seq_idx < node.size(); seq_idx++) {
			if (seq_idx > 0) {
				result += ",";
			}
			result += YAMLNodeToJSONImpl(node[seq_idx], budget);
		}
		result += "]";
		return result;
	}
	case YAML::NodeType::Map: {
		std::string result = "{";
		bool first = true;
		for (const auto &it : node) {
			if (!first) {
				result += ",";
			}
			first = false;

			// Key must be a string in JSON — escape it (a key containing " or \ would otherwise
			// produce invalid JSON; issue #42).
			const auto key = it.first.Scalar();
			result += EscapeJSONString(key) + ":" + YAMLNodeToJSONImpl(it.second, budget);
		}
		result += "}";
		return result;
	}
	default:
		throw InternalException("Unknown YAML node type");
	}
}

std::string YAMLNodeToJSON(const YAML::Node &node) {
	YAMLTraversalBudget budget;
	return YAMLNodeToJSONImpl(node, budget);
}

//===--------------------------------------------------------------------===//
// DuckDB Value to YAML Conversion
//===--------------------------------------------------------------------===//

static void EmitValueToYAMLImpl(YAML::Emitter &out, const Value &value, YAMLTraversalBudget &budget) {
	YAMLBudgetScope scope(budget);
	try {
		// Handle NULL values within data structures (emit YAML null '~')
		// Note: Top-level NULL inputs are handled at the function level following SQL semantics
		if (value.IsNull()) {
			out << YAML::Null;
			return;
		}

		switch (value.type().id()) {
		case LogicalTypeId::VARCHAR: {
			// Check if this is a JSON type (VARCHAR with JSON alias)
			if (value.type().IsJSONType()) {
				try {
					// Get the JSON string representation
					const auto json_str = value.GetValue<string>();

					// Parse JSON using YAML parser (yaml-cpp can parse JSON)
					YAML::Node json_node = YAML::Load(json_str);

					// Emit the parsed structure as YAML
					out << json_node;
				} catch (...) {
					// If JSON parsing fails, emit as quoted string
					out << YAML::SingleQuoted << value.ToString();
				}
				break;
			}

			// Handle regular VARCHAR
			const auto str_val = value.GetValue<string>();

			// Check if the string needs special formatting
			bool needs_quotes = false;

			if (str_val.empty()) {
				needs_quotes = true;
			} else {
				// Check for special strings that could be interpreted as something else
				if (str_val == "null" || str_val == "true" || str_val == "false" || str_val == "yes" ||
				    str_val == "no" || str_val == "on" || str_val == "off" || str_val == "~" || str_val == "") {
					needs_quotes = true;
				}

				// Check if it looks like a number using DuckDB's Value casting
				try {
					Value string_val(str_val);
					Value double_val = string_val.DefaultCastAs(LogicalType::DOUBLE);
					// If casting succeeded, it looks like a number
					needs_quotes = true;
				} catch (...) {
					// Not a number
				}

				// Check for special characters
				for (char ch : str_val) {
					if (ch == ':' || ch == '{' || ch == '}' || ch == '[' || ch == ']' || ch == ',' || ch == '&' ||
					    ch == '*' || ch == '#' || ch == '?' || ch == '|' || ch == '-' || ch == '<' || ch == '>' ||
					    ch == '=' || ch == '!' || ch == '%' || ch == '@' || ch == '\\' || ch == '"' || ch == '\'' ||
					    ch == '\n' || ch == '\t' || ch == ' ') {
						needs_quotes = true;
						break;
					}
				}
			}

			if (needs_quotes) {
				// Use single quoted style for most strings requiring quotes
				out << YAML::SingleQuoted << str_val;
			} else {
				// Use plain style for strings that don't need quotes
				out << str_val;
			}
			break;
		}
		case LogicalTypeId::BOOLEAN:
			out << value.GetValue<bool>();
			break;
		case LogicalTypeId::INTEGER:
		case LogicalTypeId::BIGINT:
			try {
				out << value.GetValue<int64_t>();
			} catch (...) {
				// If casting fails, convert to string as fallback
				out << YAML::SingleQuoted << value.ToString();
			}
			break;
		case LogicalTypeId::FLOAT:
		case LogicalTypeId::DOUBLE:
			try {
				out << value.GetValue<double>();
			} catch (...) {
				// If casting fails, convert to string as fallback
				out << YAML::SingleQuoted << value.ToString();
			}
			break;
		case LogicalTypeId::LIST: {
			try {
				out << YAML::BeginSeq;
				const auto &list_val = ListValue::GetChildren(value);
				for (const auto &element : list_val) {
					EmitValueToYAMLImpl(out, element, budget);
				}
				out << YAML::EndSeq;
			} catch (...) {
				// If list processing fails, emit as string
				out << YAML::SingleQuoted << value.ToString();
			}
			break;
		}
		case LogicalTypeId::STRUCT: {
			try {
				out << YAML::BeginMap;
				const auto &struct_vals = StructValue::GetChildren(value);
				const auto &struct_names = StructType::GetChildTypes(value.type());

				// Safety check for struct children
				if (struct_vals.size() != struct_names.size()) {
					throw std::runtime_error("Mismatch between struct values and names");
				}

				for (idx_t field_idx = 0; field_idx < struct_vals.size(); field_idx++) {
					out << YAML::Key << CompatIdentifierName(struct_names[field_idx].first);
					out << YAML::Value;
					EmitValueToYAMLImpl(out, struct_vals[field_idx], budget);
				}
				out << YAML::EndMap;
			} catch (...) {
				// If struct processing fails, emit as string
				out << YAML::SingleQuoted << value.ToString();
			}
			break;
		}
		default:
			// For types we don't handle specifically, convert to string
			out << YAML::SingleQuoted << value.ToString();
			break;
		}
	} catch (...) {
		// Last-resort fallback - if anything goes wrong, emit null
		try {
			out << YAML::Null;
		} catch (...) {
			// Prevent cascading exceptions
		}
	}
}

void EmitValueToYAML(YAML::Emitter &out, const Value &value) {
	YAMLTraversalBudget budget;
	EmitValueToYAMLImpl(out, value, budget);
}

// Convert DuckDB Value to YAML::Node (respects emitter configuration)
static YAML::Node ValueToYAMLNodeImpl(const Value &value, YAMLTraversalBudget &budget) {
	YAMLBudgetScope scope(budget);
	if (value.IsNull()) {
		return YAML::Node(YAML::NodeType::Null);
	}

	switch (value.type().id()) {
	case LogicalTypeId::ENUM: {
		YAML::Node enum_node(value.ToString());
		enum_node.SetTag("!");
		return enum_node;
	}
	case LogicalTypeId::VARCHAR: {
		if (value.type().IsJSONType()) {
			try {
				std::string json_str = value.GetValue<string>();
				return YAML::Load(json_str); // Parse JSON as YAML
			} catch (...) {
				return YAML::Node(value.ToString());
			}
		}
		if (value.type().HasAlias() && value.type().GetAlias() == "yaml") {
			// Already YAML, so write it as itself. Several documents become a sequence, as in yaml_to_json.
			try {
				const auto docs = ParseYAML(value.GetValue<string>(), true);
				if (docs.empty()) {
					return YAML::Node(YAML::NodeType::Null);
				}
				if (docs.size() == 1) {
					return docs[0];
				}
				YAML::Node doc_list(YAML::NodeType::Sequence);
				for (const auto &doc : docs) {
					doc_list.push_back(doc);
				}
				return doc_list;
			} catch (...) {
				return YAML::Node(value.ToString());
			}
		}
		// A string, so tag it as one: EmitNodeWithStringStyle quotes it if plain output would read back differently.
		YAML::Node string_node(value.ToString());
		string_node.SetTag("!");
		return string_node;
	}
	case LogicalTypeId::BOOLEAN:
		return YAML::Node(value.GetValue<bool>());
	case LogicalTypeId::TINYINT:
		return YAML::Node(static_cast<int>(value.GetValue<int8_t>()));
	case LogicalTypeId::SMALLINT:
		return YAML::Node(static_cast<int>(value.GetValue<int16_t>()));
	case LogicalTypeId::INTEGER:
		return YAML::Node(value.GetValue<int32_t>());
	case LogicalTypeId::BIGINT:
		return YAML::Node(value.GetValue<int64_t>());
	case LogicalTypeId::FLOAT:
		return YAML::Node(value.GetValue<float>());
	case LogicalTypeId::DOUBLE:
		return YAML::Node(value.GetValue<double>());
	case LogicalTypeId::LIST: {
		YAML::Node list_node(YAML::NodeType::Sequence);
		const auto &list_vals = ListValue::GetChildren(value);
		for (const auto &element : list_vals) {
			list_node.push_back(ValueToYAMLNodeImpl(element, budget)); // Recursive
		}
		return list_node;
	}
	case LogicalTypeId::STRUCT: {
		YAML::Node map_node(YAML::NodeType::Map);
		const auto &struct_vals = StructValue::GetChildren(value);
		const auto &struct_names = StructType::GetChildTypes(value.type());

		for (idx_t field_idx = 0; field_idx < struct_vals.size() && field_idx < struct_names.size(); field_idx++) {
			const string key = CompatIdentifierName(struct_names[field_idx].first);
			map_node[key] = ValueToYAMLNodeImpl(struct_vals[field_idx], budget); // Recursive
		}
		return map_node;
	}
	default:
		return YAML::Node(value.ToString());
	}
}

YAML::Node ValueToYAMLNode(const Value &value) {
	YAMLTraversalBudget budget;
	return ValueToYAMLNodeImpl(value, budget);
}

std::string ValueToYAMLString(const Value &value, YAMLFormat format, YAMLStringStyle string_style, idx_t indent) {
	try {
		// Convert to YAML::Node first (this respects emitter configuration)
		YAML::Node node = ValueToYAMLNode(value);

		// Now emit with proper format settings and string style
		YAML::Emitter out;
		ConfigureEmitter(out, format, indent);
		const auto resolved = ResolveStringStyle(string_style, format);
		EmitNodeWithStringStyle(out, node, resolved);

		// Check if we have a valid YAML string
		if (out.good() && out.c_str() != nullptr) {
			return resolved == YAMLStringStyle::LITERAL ? ApplyLiteralStripMarkers(out.c_str())
			                                            : std::string(out.c_str());
		} else {
			// If emitter is in error state, return null as fallback
			return "null";
		}
	} catch (const std::exception &e) {
		// Handle known exceptions
		return "null";
	} catch (...) {
		// Handle any other unexpected errors
		return "null";
	}
}

std::string FormatPerStyleAndLayout(const Value &value, YAMLFormat format, const std::string &layout) {
	// Simply return the formatted YAML string - let callers handle layout-specific formatting
	return ValueToYAMLString(value, format);
}

} // namespace yaml_utils

} // namespace duckdb
