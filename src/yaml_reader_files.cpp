#include "yaml_reader.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/enums/file_glob_options.hpp"
#include <sstream>

// Detect DuckDB v1.5+ via a header that only exists in v1.5
// v1.4: GlobFiles(pattern, context, FileGlobOptions)
// v1.5: GlobFiles(pattern, FileGlobOptions) - context parameter removed
#if __has_include("duckdb/common/column_index_map.hpp")
#define DUCKDB_GLOB_V15
#endif

namespace duckdb {

// Strip non-standard suffixes from YAML document headers (issue #34)
// Transforms: "--- !tag &anchor suffix" -> "--- !tag &anchor"
// This enables parsing of files with custom document annotations like Unity's "stripped" keyword
string YAMLReader::StripDocumentSuffixes(const string &yaml_content) {
	std::istringstream input(yaml_content);
	std::ostringstream output;
	string line;
	bool first_line = true;

	while (std::getline(input, line)) {
		if (!first_line) {
			output << '\n';
		}
		first_line = false;

		// Check if this is a document header line (starts with ---)
		if (line.size() >= 3 && line[0] == '-' && line[1] == '-' && line[2] == '-') {
			// Parse the header: --- [!tag] [&anchor] [suffix]
			// We want to keep --- !tag &anchor but remove any suffix

			size_t pos = 3; // Start after ---

			// Skip whitespace after ---
			while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
				pos++;
			}

			// Check for tag (starts with !)
			size_t tag_end = pos;
			if (pos < line.size() && line[pos] == '!') {
				// Find end of tag (next whitespace)
				while (tag_end < line.size() && line[tag_end] != ' ' && line[tag_end] != '\t') {
					tag_end++;
				}
				pos = tag_end;

				// Skip whitespace after tag
				while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
					pos++;
				}
			}

			// Check for anchor (starts with &)
			size_t anchor_end = pos;
			if (pos < line.size() && line[pos] == '&') {
				// Find end of anchor (next whitespace)
				while (anchor_end < line.size() && line[anchor_end] != ' ' && line[anchor_end] != '\t') {
					anchor_end++;
				}
				pos = anchor_end;
			}

			// If there's anything after the anchor, it's a suffix we need to strip
			// Output only up to the anchor
			if (anchor_end > 3) {
				// We found tag/anchor, output up to anchor_end
				output << line.substr(0, anchor_end);
			} else if (tag_end > 3) {
				// We found only a tag, output up to tag_end
				output << line.substr(0, tag_end);
			} else {
				// Just "---" possibly with trailing content - keep just "---"
				// But check if there's actual content (like "--- {a: 1}")
				// Skip whitespace to see what's next
				while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
					pos++;
				}
				if (pos < line.size() && line[pos] != '!' && line[pos] != '&') {
					// There's content on the same line, could be inline document
					// Check if it looks like a bare word (suffix) vs actual YAML content
					size_t word_end = pos;
					while (word_end < line.size() && line[word_end] != ' ' && line[word_end] != '\t' &&
					       line[word_end] != ':' && line[word_end] != '{' && line[word_end] != '[') {
						word_end++;
					}
					// If the word doesn't have YAML structure chars, it's likely a suffix
					if (word_end < line.size() &&
					    (line[word_end] == ':' || line[word_end] == '{' || line[word_end] == '[')) {
						// Looks like actual YAML content, keep the line as-is
						output << line;
					} else if (word_end == line.size()) {
						// Just a bare word at end of line - strip it
						output << "---";
					} else {
						// Keep the line as-is (complex case)
						output << line;
					}
				} else {
					// No content after ---, keep as-is
					output << line;
				}
			}
		} else {
			// Not a document header line, keep as-is
			output << line;
		}
	}

	return output.str();
}

// Helper function to get files from a Value (which can be a string or list of strings)
vector<string> YAMLReader::GetFiles(ClientContext &context, const Value &path_value, bool ignore_errors) {
	auto &fs = FileSystem::GetFileSystem(context);
	vector<string> result;

	// Helper lambda to handle individual file paths
	auto processPath = [&](const string &yaml_path) {
		// First: check if we're dealing with just a single file that exists
		if (fs.FileExists(yaml_path)) {
			result.push_back(yaml_path);
			return;
		}

		// Second: attempt to use the path as a glob
		auto glob_files = GetGlobFiles(context, yaml_path);
		if (glob_files.size() > 0) {
			result.insert(result.end(), glob_files.begin(), glob_files.end());
			return;
		}

		// Third: if it looks like a directory, try to glob out all of the yaml children
		if (StringUtil::EndsWith(yaml_path, "/")) {
			auto yaml_files = GetGlobFiles(context, fs.JoinPath(yaml_path, "*.yaml"));
			auto yml_files = GetGlobFiles(context, fs.JoinPath(yaml_path, "*.yml"));
			result.insert(result.end(), yaml_files.begin(), yaml_files.end());
			result.insert(result.end(), yml_files.begin(), yml_files.end());
			return;
		}

		if (ignore_errors) {
			return;
		} else if (yaml_path.find("://") != string::npos && yaml_path.find("file://") != 0) {
			throw IOException("Remote file does not exist or is not accessible: " + yaml_path);
		} else {
			throw IOException("File or directory does not exist: " + yaml_path);
		}
	};

	// Handle list of files
	if (path_value.type().id() == LogicalTypeId::LIST) {
		auto &file_list = ListValue::GetChildren(path_value);
		for (auto &file_value : file_list) {
			if (file_value.type().id() == LogicalTypeId::VARCHAR) {
				processPath(file_value.ToString());
			} else {
				throw BinderException("File list must contain string values");
			}
		}
		// Handle string path (file, glob pattern, or directory)
	} else if (path_value.type().id() == LogicalTypeId::VARCHAR) {
		processPath(path_value.ToString());
		// Handle invalid types
	} else {
		throw BinderException("File path must be a string or list of strings");
	}

	return result;
}

// Helper functions for file globbing and file list handling
vector<string> YAMLReader::GetGlobFiles(ClientContext &context, const string &pattern) {
	auto &fs = FileSystem::GetFileSystem(context);
	vector<string> result;

	// Don't bother if we can't identify a glob pattern
	// For S3 URLs, we'll let GlobFiles handle the detection
	try {
		bool has_glob = fs.HasGlob(pattern);
		if (!has_glob) {
			// For remote URLs, still try GlobFiles as it has better S3 support
			if (pattern.find("://") == string::npos || pattern.find("file://") == 0) {
				return result;
			}
		}
	} catch (const NotImplementedException &) {
		// If HasGlob is not implemented, still try GlobFiles for remote URLs
		if (pattern.find("://") == string::npos || pattern.find("file://") == 0) {
			return result;
		}
	}

	// GlobFiles already handles filtering out directories, so we don't need to check

	// Use GlobFiles which handles extension auto-loading and directory filtering
	try {
#ifdef DUCKDB_GLOB_V15
		auto glob_files = fs.GlobFiles(pattern, FileGlobOptions::ALLOW_EMPTY);
		for (auto &file : glob_files) {
			result.push_back(file.path);
		}
#else
		auto glob_files = fs.GlobFiles(pattern, context, FileGlobOptions::ALLOW_EMPTY);
		for (auto &file : glob_files) {
			result.push_back(file.path);
		}
#endif
	} catch (const NotImplementedException &) {
		// No glob support available
	} catch (const std::exception &) {
		// Other glob errors - return empty result
	}

	return result;
}

// Helper to read a single file and parse it
vector<YAML::Node> YAMLReader::ReadYAMLFile(ClientContext &context, const string &file_path,
                                            const YAMLReadOptions &options) {
	auto &fs = FileSystem::GetFileSystem(context);

	// Check if file exists
	if (!fs.FileExists(file_path)) {
		throw IOException("File does not exist: " + file_path);
	}

	auto handle = fs.OpenFile(file_path, FileFlags::FILE_FLAGS_READ);
	idx_t file_size = fs.GetFileSize(*handle);

	if (file_size > options.maximum_object_size) {
		throw IOException("YAML file size (" + to_string(file_size) + " bytes) exceeds maximum allowed size (" +
		                  to_string(options.maximum_object_size) + " bytes)");
	}

	// Read the file content
	string content(file_size, ' ');
	fs.Read(*handle, const_cast<char *>(content.c_str()), file_size);

	// Strip non-standard document suffixes if enabled (issue #34)
	// This allows parsing files with custom annotations like Unity's "stripped" keyword
	if (options.strip_document_suffixes) {
		content = StripDocumentSuffixes(content);
	}

	vector<YAML::Node> docs;

	if (options.multi_document_mode != MultiDocumentMode::FIRST) {
		try {
			// First try to parse the entire file at once
			std::stringstream yaml_stream(content);
			docs = YAML::LoadAll(yaml_stream);
		} catch (const YAML::Exception &e) {
			if (!options.ignore_errors) {
				throw IOException("Error parsing multi-document YAML file: " + string(e.what()));
			}

			// On error with ignore_errors=true, try to recover partial documents
			docs = RecoverPartialYAMLDocuments(content);
		}
	} else {
		// Parse as single-document YAML
		try {
			YAML::Node yaml_node = YAML::Load(content);
			docs.push_back(yaml_node);
		} catch (const YAML::Exception &e) {
			if (!options.ignore_errors) {
				throw IOException("Error parsing YAML file: " + string(e.what()));
			}
			// With ignore_errors=true for single doc, we can try to parse it more leniently
			auto recovered = RecoverPartialYAMLDocuments(content);
			if (!recovered.empty()) {
				docs = recovered;
			}
		}
	}

	return docs;
}

} // namespace duckdb
