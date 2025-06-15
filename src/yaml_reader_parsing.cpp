#include "yaml_reader.hpp"
#include "duckdb/common/string_util.hpp"

namespace duckdb {

// Helper function for parsing multi-document YAML with error recovery
vector<YAML::Node> YAMLReader::ParseMultiDocumentYAML(const string &yaml_content, bool ignore_errors) {
	std::stringstream yaml_stream(yaml_content);

	vector<YAML::Node> valid_docs;
	try {
		valid_docs = YAML::LoadAll(yaml_stream);
		return valid_docs;
	} catch (const YAML::Exception &e) {
		if (!ignore_errors) {
			throw IOException("Error parsing YAML file: " + string(e.what()));
		}

		// On error with ignore_errors=true, try to recover partial documents
		return RecoverPartialYAMLDocuments(yaml_content);
	}
}

// Helper function for extracting row nodes
vector<YAML::Node> YAMLReader::ExtractRowNodes(const vector<YAML::Node> &docs, bool expand_root_sequence) {
	vector<YAML::Node> row_nodes;

	for (const auto &doc : docs) {
		if (doc.IsSequence() && expand_root_sequence) {
			// Each item in the sequence becomes a row
			for (size_t idx = 0; idx < doc.size(); idx++) {
				if (doc[idx].IsMap()) { // Only add map nodes as rows
					row_nodes.push_back(doc[idx]);
				}
			}
		} else if (doc.IsMap()) {
			// Document itself becomes a row
			row_nodes.push_back(doc);
		}
	}

	return row_nodes;
}

// Helper function to recover partial valid documents from YAML with syntax errors
vector<YAML::Node> YAMLReader::RecoverPartialYAMLDocuments(const string &yaml_content) {
	vector<YAML::Node> valid_docs;

	// First, we'll try to identify document separators and split the content
	vector<string> doc_strings;

	// Normalize newlines
	string normalized_content = yaml_content;
	StringUtil::Replace(normalized_content, "\r\n", "\n");

	// Find all document separators
	size_t pos = 0;
	size_t prev_pos = 0;
	const string doc_separator = "\n---";

	// Handle if the file starts with a document separator
	if (normalized_content.compare(0, 3, "---") == 0) {
		prev_pos = 0;
	} else {
		// First document doesn't start with separator
		pos = normalized_content.find(doc_separator);
		if (pos != string::npos) {
			// Extract the first document
			doc_strings.push_back(normalized_content.substr(0, pos));
			prev_pos = pos + 1; // +1 to include the newline
		} else {
			// No separators found, treat the whole file as one document
			doc_strings.push_back(normalized_content);
		}
	}

	// Find all remaining document separators
	while ((pos = normalized_content.find(doc_separator, prev_pos)) != string::npos) {
		// Extract document between the current and next separator
		string doc = normalized_content.substr(prev_pos, pos - prev_pos);

		// Only add non-empty documents
		if (!doc.empty()) {
			doc_strings.push_back(doc);
		}

		prev_pos = pos + doc_separator.length();
	}

	// Add the last document if there's content after the last separator
	if (prev_pos < normalized_content.length()) {
		doc_strings.push_back(normalized_content.substr(prev_pos));
	}

	// If we didn't find any document separators, try the whole content as one document
	if (doc_strings.empty()) {
		doc_strings.push_back(normalized_content);
	}

	// Try to parse each document individually
	for (const auto &doc_str : doc_strings) {
		try {
			// Skip empty documents or just whitespace/comments
			string trimmed = doc_str;
			StringUtil::Trim(trimmed);
			if (trimmed.empty() || trimmed[0] == '#') {
				continue;
			}

			// Add separator back for proper YAML parsing if it's not the first document
			string to_parse = doc_str;
			if (to_parse.compare(0, 3, "---") != 0 && &doc_str != &doc_strings[0]) {
				to_parse = "---" + to_parse;
			}

			YAML::Node doc = YAML::Load(to_parse);
			// Check if we got a valid node
			if (doc.IsDefined() && !doc.IsNull()) {
				valid_docs.push_back(doc);
			}
		} catch (const YAML::Exception &) {
			// Skip invalid documents
			continue;
		}
	}

	return valid_docs;
}

} // namespace duckdb