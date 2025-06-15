#include "yaml_reader.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"

namespace duckdb {

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
	bool supports_directory_exists;
	bool is_directory;

	// Don't both if we can't identify a glob pattern
	try {
		if (!fs.HasGlob(pattern)) {
			return result;
		}
	} catch (const NotImplementedException &) {
		return result;
	}

	// Check this once up-front and save the FS feature
	try {
		is_directory = fs.DirectoryExists(pattern);
		supports_directory_exists = true;
	} catch (const NotImplementedException &) {
		is_directory = false;
		supports_directory_exists = false;
	}

	// Given a glob path, add any file results (ignoring directories)
	try {
		for (auto &file : fs.Glob(pattern)) {
			if (!supports_directory_exists) {
				// If we can't check for directories, just add it
				result.push_back(file.path);
			} else if (!fs.DirectoryExists(file.path)) {
				result.push_back(file.path);
			}
		}
	} catch (const NotImplementedException &) {
		// No glob support
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

	vector<YAML::Node> docs;

	if (options.multi_document) {
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