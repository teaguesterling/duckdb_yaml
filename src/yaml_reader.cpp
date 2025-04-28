#include "yaml_reader.hpp"
#include "duckdb/catalog/catalog_entry/table_function_catalog_entry.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/main/extension_util.hpp"

// This is a hack to deal with some weird inconsistencies between the result type of fs.GlobFiles
#if (__GNUC__ >= 5) || defined(__clang__)
#define HACK_GLOB_PATH(x) x.path
#else
#define HACK_GLOB_PATH(x) x
#endif

namespace duckdb {

// Bind data structure for read_yaml
struct YAMLReadRowsBindData : public TableFunctionData {
    YAMLReadRowsBindData(string file_path, YAMLReader::YAMLReadOptions options)
        : file_path(std::move(file_path)), options(options) {}

    string file_path;
    YAMLReader::YAMLReadOptions options;
    vector<YAML::Node> yaml_docs;  // Each document in the YAML file
    vector<string> names;          // Column names
    vector<LogicalType> types;     // Column types
    idx_t current_doc = 0;         // Current document being processed
};


// Bind data structure for read_yaml_objects
struct YAMLReadBindData : public TableFunctionData {
    YAMLReadBindData(string file_path, YAMLReader::YAMLReadOptions options)
        : file_path(std::move(file_path)), options(options) {}

    string file_path;
    YAMLReader::YAMLReadOptions options;
    vector<YAML::Node> yaml_docs;
    vector<string> names;
    vector<LogicalType> types;
    idx_t current_row = 0;
};

// YAML Type Conversions
// Helper function to detect YAML type
LogicalType YAMLReader::DetectYAMLType(const YAML::Node &node) {
    if (!node) {
        return LogicalType::VARCHAR;
    }

    switch (node.Type()) {
        case YAML::NodeType::Scalar: {
            std::string scalar_value = node.Scalar();

            // Basic type detection
            if (scalar_value == "true" || scalar_value == "false" || 
                scalar_value == "yes" || scalar_value == "no") {
                return LogicalType::BOOLEAN;
            }

            try {
                // Try integer
                size_t pos;
                std::stoll(scalar_value, &pos);
                if (pos == scalar_value.size()) {
                    return LogicalType::BIGINT;
                }

                // Try double
                std::stod(scalar_value, &pos);
                if (pos == scalar_value.size()) {
                    return LogicalType::DOUBLE;
                }
            } catch (...) {
                // Not a number
            }

            return LogicalType::VARCHAR;
        }
        case YAML::NodeType::Sequence: {
            if (node.size() == 0) {
                return LogicalType::LIST(LogicalType::VARCHAR);
            }

            // Check first element to determine list type
            YAML::Node first_element = node[0];
            LogicalType element_type = DetectYAMLType(first_element);

            // If the sequence contains maps, they should be a list of structs
            if (first_element.IsMap()) {
                // The element_type will already be a struct type from the recursive call
            }

            return LogicalType::LIST(element_type);
        }
        case YAML::NodeType::Map: {
            child_list_t<LogicalType> struct_children;
            for (auto it = node.begin(); it != node.end(); ++it) {
                std::string key = it->first.Scalar();
                LogicalType value_type = DetectYAMLType(it->second);
                struct_children.push_back(make_pair(key, value_type));
            }
            return LogicalType::STRUCT(struct_children);
        }
        default:
            return LogicalType::VARCHAR;
    }
}

// Helper function to convert YAML node to DuckDB value
Value YAMLReader::YAMLNodeToValue(const YAML::Node &node, const LogicalType &target_type) {
    if (!node) {
        return Value(target_type); // NULL value
    }

    // Handle based on YAML node type
    switch (node.Type()) {
        case YAML::NodeType::Scalar: {
            std::string scalar_value = node.Scalar();

            if (target_type.id() == LogicalTypeId::VARCHAR) {
                return Value(scalar_value);
            } else if (target_type.id() == LogicalTypeId::BOOLEAN) {
                if (scalar_value == "true" || scalar_value == "yes" || scalar_value == "on") {
                    return Value::BOOLEAN(true);
                } else if (scalar_value == "false" || scalar_value == "no" || scalar_value == "off") {
                    return Value::BOOLEAN(false);
                }
                return Value(target_type); // NULL if not valid boolean
            } else if (target_type.id() == LogicalTypeId::BIGINT) {
                try {
                    return Value::BIGINT(std::stoll(scalar_value));
                } catch (...) {
                    return Value(target_type); // NULL if conversion fails
                }
            } else if (target_type.id() == LogicalTypeId::DOUBLE) {
                try {
                    return Value::DOUBLE(std::stod(scalar_value));
                } catch (...) {
                    return Value(target_type); // NULL if conversion fails
                }
            }
            return Value(scalar_value); // Default to string
        }
        case YAML::NodeType::Sequence: {
            if (target_type.id() != LogicalTypeId::LIST) {
                return Value(target_type); // NULL if not expecting a list
            }

            // Get child type for list
            auto child_type = ListType::GetChildType(target_type);

            // Create list of values - recursively convert each element
            vector<Value> values;
            for (size_t i = 0; i < node.size(); i++) {
                values.push_back(YAMLNodeToValue(node[i], child_type));
            }

            return Value::LIST(values);
        }
        case YAML::NodeType::Map: {
            if (target_type.id() != LogicalTypeId::STRUCT) {
                return Value(target_type); // NULL if not expecting a struct
            }

            // Get struct children
            auto &struct_children = StructType::GetChildTypes(target_type);

            // Create struct values
            child_list_t<Value> struct_values;
            for (auto &entry : struct_children) {
                if (node[entry.first]) {
                    struct_values.push_back(make_pair(
                        entry.first, 
                        YAMLNodeToValue(node[entry.first], entry.second)
                    ));
                } else {
                    struct_values.push_back(make_pair(
                        entry.first, 
                        Value(entry.second) // NULL value
                    ));
                }
            }

            return Value::STRUCT(struct_values);
        }
        default:
            return Value(target_type); // NULL for unknown type
    }
}

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
            for (size_t i = 0; i < doc.size(); i++) {
                if (doc[i].IsMap()) { // Only add map nodes as rows
                    row_nodes.push_back(doc[i]);
                }
            }
        } else if (doc.IsMap()) {
            // Document itself becomes a row
            row_nodes.push_back(doc);
        }
    }
    
    return row_nodes;
}

// Helper functions for file globbing and file list handling
vector<string> YAMLReader::GlobFiles(ClientContext &context, const Value &path_value, bool only_existing) {
    auto &fs = FileSystem::GetFileSystem(context);
    vector<string> result;
    
    // Handle list of files
    if (path_value.type().id() == LogicalTypeId::LIST) {
        auto &file_list = ListValue::GetChildren(path_value);
        for (auto &file_value : file_list) {
            if (file_value.type().id() != LogicalTypeId::VARCHAR) {
                throw BinderException("File list must contain string values");
            }
            
            string file_path = file_value.ToString();
            if (fs.FileExists(file_path)) {
                result.push_back(file_path);
            } else if (!only_existing) {
                throw IOException("File does not exist: " + file_path);
            }
        }
    } 
    // Handle string path (file, glob pattern, or directory)
    else if (path_value.type().id() == LogicalTypeId::VARCHAR) {
        string path = path_value.ToString();
        
        // Handle glob patterns
        if (path.find('*') != string::npos || path.find('?') != string::npos) {
            auto globbed_files = fs.GlobFiles(path, context);
            for (auto &file : globbed_files) {
                // Skip directories
                if (!fs.DirectoryExists(HACK_GLOB_PATH(file))) {
                    result.push_back(HACK_GLOB_PATH(file));
                }
            }
        } 
        // Handle directory
        else if (fs.DirectoryExists(path)) {
            // Get all .yaml files
            auto yaml_files = fs.GlobFiles(path + "/*.yaml", context);
            for (auto &file : yaml_files) {
                if (!fs.DirectoryExists(HACK_GLOB_PATH(file))) {
                    result.push_back(HACK_GLOB_PATH(file));
                }
            }
            
            // Also get .yml files
            auto yml_files = fs.GlobFiles(path + "/*.yml", context);
            for (auto &file : yml_files) {
                if (!fs.DirectoryExists(HACK_GLOB_PATH(file))) {
                    result.push_back(HACK_GLOB_PATH(file));
                }
            }
        }
        // Handle single file
        else if (fs.FileExists(path)) {
            result.push_back(path);
        } 
        else if(!only_existing) {
            throw IOException("File or directory does not exist: " + path);
        }
    } 
    else {
        throw BinderException("File path must be a string or list of strings");
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
    fs.Read(*handle, const_cast<char*>(content.c_str()), file_size);

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

unique_ptr<FunctionData> YAMLReader::YAMLReadRowsBind(ClientContext &context,
                                                  TableFunctionBindInput &input,
                                                  vector<LogicalType> &return_types,
                                                  vector<string> &names) {
    // Validate primary input
    if (input.inputs.empty()) {
        throw BinderException("read_yaml requires a file path parameter");
    }

    // Extract the file path parameter
    Value path_value = input.inputs[0];
    string file_path;

    // For display purposes and bind data, we'll use a string representation
    if (path_value.type().id() == LogicalTypeId::VARCHAR) {
        file_path = path_value.ToString();
    } else if (path_value.type().id() == LogicalTypeId::LIST) {
        // For lists, use the first file for the bind data's file_path (just for reference)
        auto &file_list = ListValue::GetChildren(path_value);
        if (!file_list.empty()) {
            file_path = file_list[0].ToString() + " and others";
        } else {
            throw BinderException("File list cannot be empty");
        }
    } else {
        throw BinderException("File path must be a string or list of strings");
    }

    YAMLReadOptions options;

    // Check for duplicate parameters
    std::unordered_set<std::string> seen_parameters;
    for (auto& param : input.named_parameters) {
        if (seen_parameters.find(param.first) != seen_parameters.end()) {
            throw BinderException("Duplicate parameter name: " + param.first);
        }
        seen_parameters.insert(param.first);
    }

    // Parse optional parameters
    if (seen_parameters.find("auto_detect") != seen_parameters.end()) {
        options.auto_detect_types = input.named_parameters["auto_detect"].GetValue<bool>();
    }
    if (seen_parameters.find("ignore_errors") != seen_parameters.end()) {
        options.ignore_errors = input.named_parameters["ignore_errors"].GetValue<bool>();
    }
    if (seen_parameters.find("maximum_object_size") != seen_parameters.end()) {
        options.maximum_object_size = input.named_parameters["maximum_object_size"].GetValue<int64_t>();
        if (options.maximum_object_size <= 0) {
            throw BinderException("maximum_object_size must be a positive integer");
        }
    }
    if (seen_parameters.find("multi_document") != seen_parameters.end()) {
        options.multi_document = input.named_parameters["multi_document"].GetValue<bool>();
    }
    if (seen_parameters.find("expand_root_sequence") != seen_parameters.end()) {
        options.expand_root_sequence = input.named_parameters["expand_root_sequence"].GetValue<bool>();
    }

    // Create bind data
    auto result = make_uniq<YAMLReadRowsBindData>(file_path, options);

    // Get files using globbing
    auto files = GlobFiles(context, path_value, options.ignore_errors);
    if (files.empty() && !options.ignore_errors) {
        throw IOException("No YAML files found matching the input path");
    }

    // Vector to store all the row data items (documents or elements)
    vector<YAML::Node> row_nodes;

    // Read and process all files
    for (const auto& f : files) {
        try {
            auto docs = ReadYAMLFile(context, f, options);
            auto file_nodes = ExtractRowNodes(docs, options.expand_root_sequence);

            // Add nodes from this file
            row_nodes.insert(row_nodes.end(), file_nodes.begin(), file_nodes.end());
        } catch (const std::exception &e) {
            if (!options.ignore_errors) {
                throw IOException("Error processing YAML file '" + f + "': " + string(e.what()));
            }
            // With ignore_errors=true, we allow continuing with other files
        }
    }

    // Replace the docs with our processed row_nodes
    result->yaml_docs = row_nodes;

    // Handle empty result set early
    // TODO: This is very messy and could probably be drastically simplified
    // or at the very least, moved into a helper function
    if (result->yaml_docs.empty()) {
        if (options.ignore_errors) {
            // With ignore_errors=true, return an empty table with a dummy structure
            // that matches what would be expected if data existed
            if (path_value.type().id() == LogicalTypeId::LIST) {
                // For file lists, try to infer schema from files that exist
                for (const auto& file_val : ListValue::GetChildren(path_value)) {
                    string file = file_val.ToString();
                    if (FileSystem::GetFileSystem(context).FileExists(file)) {
                        // Try to read this file to infer schema
                        try {
                            auto docs = ReadYAMLFile(context, file, options);
                            if (!docs.empty() && docs[0].IsMap()) {
                                // Use the first valid document to define columns
                                for (auto it = docs[0].begin(); it != docs[0].end(); ++it) {
                                    string key = it->first.Scalar();
                                    LogicalType type;
                                    if (options.auto_detect_types) {
                                        type = DetectYAMLType(it->second);
                                    } else {
                                        type = LogicalType::VARCHAR;
                                    }
                                    names.push_back(key);
                                    return_types.push_back(type);
                                }
                                break;
                            }
                        } catch (...) {
                            // Ignore errors when inferring schema
                        }
                    }
                }
            } else if (path_value.type().id() == LogicalTypeId::VARCHAR) {
                // For a single file/pattern, try to infer from that
                try {
                    string file = path_value.ToString();
                    // If it's a glob pattern, try to find the first matching file
                    if (file.find('*') != string::npos || file.find('?') != string::npos) {
                        auto globbed = FileSystem::GetFileSystem(context).Glob(file);
                        if (!globbed.empty()) {
                            file = globbed[0].path;
                        }
                    }

                    if (FileSystem::GetFileSystem(context).FileExists(file)) {
                        auto docs = ReadYAMLFile(context, file, options);
                        if (!docs.empty() && docs[0].IsMap()) {
                            // Use the first valid document to define columns
                            for (auto it = docs[0].begin(); it != docs[0].end(); ++it) {
                                string key = it->first.Scalar();
                                LogicalType type;
                                if (options.auto_detect_types) {
                                    type = DetectYAMLType(it->second);
                                } else {
                                    type = LogicalType::VARCHAR;
                                }
                                names.push_back(key);
                                return_types.push_back(type);
                            }
                        }
                    }
                } catch (...) {
                    // Ignore errors when inferring schema
                }
            }

            // If we still don't have columns, add a dummy column
            if (names.empty()) {
                names.emplace_back("yaml");
                return_types.emplace_back(LogicalType::VARCHAR);
            }
        } else {
            // Without ignore_errors, this is an error
            throw IOException("No valid YAML documents found");
        }

        // Save schema and return the result with empty data
        result->names = names;
        result->types = return_types;
        return std::move(result);
    }

    // Extract schema from all row nodes
    unordered_map<string, LogicalType> column_types;

    // Process each row node to build the schema
    for (auto &node : result->yaml_docs) {
        // Process each top-level key as a potential column
        for (auto it = node.begin(); it != node.end(); ++it) {
            std::string key = it->first.Scalar();
            YAML::Node value = it->second;

            // Detect the type of this value
            LogicalType value_type;
            if (options.auto_detect_types) {
                value_type = DetectYAMLType(value);
            } else {
                value_type = LogicalType::VARCHAR;
            }

            // If we already have this column, reconcile the types
            if (column_types.count(key)) {
                if (column_types[key].id() != value_type.id()) {
                    column_types[key] = LogicalType::VARCHAR;
                }
            } else {
                column_types[key] = value_type;
            }
        }
    }

    // Build the schema
    for (auto &entry : column_types) {
        names.push_back(entry.first);
        return_types.push_back(entry.second);
    }

    // Special handling for non-map documents
    if (names.empty() && !result->yaml_docs.empty()) {
        // This could happen with non-map documents without expand_root_sequence
        // Add a fallback value column
        names.emplace_back("value");
        if (options.auto_detect_types) {
            return_types.emplace_back(DetectYAMLType(result->yaml_docs[0]));
        } else {
            return_types.emplace_back(LogicalType::VARCHAR);
        }
    }

    // Save the schema
    result->names = names;
    result->types = return_types;

    return std::move(result);
}

unique_ptr<FunctionData> YAMLReader::YAMLReadBind(ClientContext &context,
                                               TableFunctionBindInput &input,
                                               vector<LogicalType> &return_types,
                                               vector<string> &names) {
    // Validate primary input
    if (input.inputs.empty()) {
        throw BinderException("read_yaml_objects requires a file path parameter");
    }

    // Extract the file path parameter
    Value path_value = input.inputs[0];
    string file_path;

    // For display purposes and bind data, we'll use a string representation
    if (path_value.type().id() == LogicalTypeId::VARCHAR) {
        file_path = path_value.ToString();
    } else if (path_value.type().id() == LogicalTypeId::LIST) {
        // For lists, use the first file for the bind data's file_path (just for reference)
        auto &file_list = ListValue::GetChildren(path_value);
        if (!file_list.empty()) {
            file_path = file_list[0].ToString() + " and others";
        } else {
            throw BinderException("File list cannot be empty");
        }
    } else {
        throw BinderException("File path must be a string or list of strings");
    }

    YAMLReadOptions options;

    // Check for duplicate parameters
    std::unordered_set<std::string> seen_parameters;
    for (auto& param : input.named_parameters) {
        if (seen_parameters.find(param.first) != seen_parameters.end()) {
            throw BinderException("Duplicate parameter name: " + param.first);
        }
        seen_parameters.insert(param.first);
    }

    // Parse optional parameters
    if (seen_parameters.find("auto_detect") != seen_parameters.end()) {
        options.auto_detect_types = input.named_parameters["auto_detect"].GetValue<bool>();
    }
    if (seen_parameters.find("ignore_errors") != seen_parameters.end()) {
        options.ignore_errors = input.named_parameters["ignore_errors"].GetValue<bool>();
    }
    if (seen_parameters.find("maximum_object_size") != seen_parameters.end()) {
        options.maximum_object_size = input.named_parameters["maximum_object_size"].GetValue<int64_t>();
        if (options.maximum_object_size <= 0) {
            throw BinderException("maximum_object_size must be a positive integer");
        }
    }
    if (seen_parameters.find("multi_document") != seen_parameters.end()) {
        options.multi_document = input.named_parameters["multi_document"].GetValue<bool>();
    }
    if (seen_parameters.find("expand_root_sequence") != seen_parameters.end()) {
        options.expand_root_sequence = input.named_parameters["expand_root_sequence"].GetValue<bool>();
    }

    // Create bind data
    auto result = make_uniq<YAMLReadBindData>(file_path, options);

    // Get files using globbing
    auto files = GlobFiles(context, path_value, options.ignore_errors);
    if (files.empty() && !options.ignore_errors) {
        throw IOException("No YAML files found matching the input path");
    }

    // Vector to store all YAML documents
    vector<YAML::Node> all_docs;

    // Read and process all files
    for (const auto& f : files) {
        try {
            auto docs = ReadYAMLFile(context, f, options);
            all_docs.insert(all_docs.end(), docs.begin(), docs.end());
        } catch (const std::exception &e) {
            if (!options.ignore_errors) {
                throw IOException("Error processing YAML file '" + f + "': " + string(e.what()));
            }
            // With ignore_errors=true, we allow continuing with other files
        }
    }

    // Store all documents
    result->yaml_docs = all_docs;

    if (result->yaml_docs.empty()) {
        // Empty result
        names.emplace_back("yaml");
        return_types.emplace_back(LogicalType::VARCHAR);
        return std::move(result);
    }

    // Auto-detect columns from first document
    if (options.auto_detect_types) {
        // For each document, we create a column with the document structure
        names.emplace_back("yaml");
        auto doc_type = DetectYAMLType(result->yaml_docs[0]);
        return_types.emplace_back(doc_type);
    } else {
        // If not auto-detecting, just use VARCHAR for the whole document
        names.emplace_back("yaml");
        return_types.emplace_back(LogicalType::VARCHAR);
    }

    // Save column info
    result->names = names;
    result->types = return_types;

    return std::move(result);
}

// Read functions

void YAMLReader::YAMLReadRowsFunction(ClientContext &context, TableFunctionInput &data_p,
                                   DataChunk &output) {
    auto &bind_data = (YAMLReadRowsBindData &)*data_p.bind_data;

    // If we've processed all rows, we're done
    if (bind_data.current_doc >= bind_data.yaml_docs.size()) {
        output.SetCardinality(0);
        return;
    }

    // Process up to STANDARD_VECTOR_SIZE rows at a time
    idx_t count = 0;
    idx_t max_count = std::min((idx_t)STANDARD_VECTOR_SIZE, bind_data.yaml_docs.size() - bind_data.current_doc);

    // Set up the output chunk
    output.Reset();

    // Special case: if we have a dummy column due to ignore_errors=true, just return empty result
    if (bind_data.names.size() == 1 &&
        (bind_data.names[0] == "yaml" && bind_data.types[0].id() == LogicalTypeId::STRUCT &&
         StructType::GetChildTypes(bind_data.types[0]).empty())) {
        // Just return empty result for dummy columns with no data
        output.SetCardinality(0);
        bind_data.current_doc = bind_data.yaml_docs.size(); // Mark as completed
        return;
    }

    // Handle value column specially (non-map documents)
    if (bind_data.names.size() == 1 && bind_data.names[0] == "value") {
        for (idx_t i = 0; i < max_count; i++) {
            // Get the current YAML node
            YAML::Node node = bind_data.yaml_docs[bind_data.current_doc + i];

            // Convert to DuckDB value
            Value val = YAMLNodeToValue(node, bind_data.types[0]);

            // Add to output
            output.SetValue(0, count, val);
            count++;
        }
    } else {
        // Normal case - process map documents
        for (idx_t i = 0; i < max_count; i++) {
            // Get the current YAML node
            YAML::Node node = bind_data.yaml_docs[bind_data.current_doc + i];

            // Process each column
            for (idx_t col_idx = 0; col_idx < bind_data.names.size(); col_idx++) {
                string &name = bind_data.names[col_idx];
                LogicalType &type = bind_data.types[col_idx];

                // Get the value for this column
                YAML::Node value = node[name];

                // Convert to DuckDB value
                Value duckdb_value;
                if (value) {
                    duckdb_value = YAMLNodeToValue(value, type);
                } else {
                    duckdb_value = Value(type); // NULL value
                }

                // Add to output
                output.SetValue(col_idx, count, duckdb_value);
            }
            count++;
        }
    }

    // Update current row
    bind_data.current_doc += count;

    // Set the cardinality
    output.SetCardinality(count);
}

void YAMLReader::YAMLReadFunction(ClientContext &context, TableFunctionInput &data_p,
                               DataChunk &output) {
    auto &bind_data = (YAMLReadBindData &)*data_p.bind_data;

    // If we've processed all rows, we're done
    if (bind_data.current_row >= bind_data.yaml_docs.size()) {
        output.SetCardinality(0);
        return;
    }

    // Process up to STANDARD_VECTOR_SIZE rows at a time
    idx_t count = 0;
    idx_t max_count = std::min((idx_t)STANDARD_VECTOR_SIZE, bind_data.yaml_docs.size() - bind_data.current_row);

    // Set up the output chunk
    output.Reset();

    // Fill the output
    for (idx_t i = 0; i < max_count; i++) {
        // Get the current YAML node
        YAML::Node node = bind_data.yaml_docs[bind_data.current_row + i];

        // Convert to DuckDB value
        Value val = YAMLNodeToValue(node, bind_data.types[0]);

        // Add to output
        output.SetValue(0, count, val);
        count++;
    }

    // Update current row
    bind_data.current_row += count;

    // Set the cardinality
    output.SetCardinality(count);
}

void YAMLReader::RegisterFunction(DatabaseInstance &db) {
    // Create read_yaml table function
    TableFunction read_yaml("read_yaml", {LogicalType::ANY}, YAMLReadRowsFunction, YAMLReadRowsBind);

    // Add optional named parameters
    read_yaml.named_parameters["auto_detect"] = LogicalType::BOOLEAN;
    read_yaml.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    read_yaml.named_parameters["maximum_object_size"] = LogicalType::BIGINT;
    read_yaml.named_parameters["multi_document"] = LogicalType::BOOLEAN;
    read_yaml.named_parameters["expand_root_sequence"] = LogicalType::BOOLEAN;

    // Register the function
    ExtensionUtil::RegisterFunction(db, read_yaml);

    // Register the object-based reader
    TableFunction read_yaml_objects("read_yaml_objects", {LogicalType::ANY}, YAMLReadFunction, YAMLReadBind);
    read_yaml_objects.named_parameters["auto_detect"] = LogicalType::BOOLEAN;
    read_yaml_objects.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    read_yaml_objects.named_parameters["maximum_object_size"] = LogicalType::BIGINT;
    read_yaml_objects.named_parameters["multi_document"] = LogicalType::BOOLEAN;
    ExtensionUtil::RegisterFunction(db, read_yaml_objects);
}

} // namespace duckdb
