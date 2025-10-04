#include "yaml_reader.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "yaml_types.hpp"

namespace duckdb {

// Column type binding helper function
void YAMLReader::BindColumnTypes(ClientContext &context, TableFunctionBindInput &input,
                                 YAMLReadOptions &options) {
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
    
    // Bind column types if specified
    BindColumnTypes(context, input, options);

    // Create bind data
    auto result = make_uniq<YAMLReadRowsBindData>(file_path, options);
    
    // Get files using globbing
    auto files = YAMLReader::GetFilePaths(context, file_path);
    if (files.empty() && !options.ignore_errors) {
        throw IOException("No YAML files found matching the input path");
    }
    
    // Vector to store all the row data items (documents or elements)
    vector<YAML::Node> row_nodes;
    
    // Read and process all files
    for (const auto& f : files) {
        try {
            auto &fs = FileSystem::GetFileSystem(context);
            auto file_content = fs.ReadFile(f);
            auto docs = YAMLReader::ParseMultiDocumentYAML(*file_content, options.ignore_errors);
            auto file_nodes = YAMLReader::ExtractRowNodes(docs, options.expand_root_sequence);
            
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
    if (result->yaml_docs.empty()) {
        if (options.ignore_errors) {
            // With ignore_errors=true, return an empty table
            if (!options.column_names.empty()) {
                // Use user-specified columns
                names = options.column_names;
                return_types = options.column_types;
            } else {
                // Default to a single VARCHAR column
                names.emplace_back("yaml");
                return_types.emplace_back(LogicalType::VARCHAR);
            }
            result->names = names;
            result->types = return_types;
            return std::move(result);
        } else {
            throw InvalidInputException("No YAML documents found");
        }
    }
    
    // Check if user specified columns
    if (!options.column_names.empty()) {
        names = options.column_names;
        return_types = options.column_types;
    } else {
        // Auto-detect schema from first document
        auto &first_doc = result->yaml_docs[0];
        
        if (first_doc.IsMap()) {
            // Extract keys as column names
            for (auto it = first_doc.begin(); it != first_doc.end(); ++it) {
                string key = it->first.Scalar();
                names.push_back(key);
                
                // Detect type if auto-detection is enabled
                if (options.auto_detect_types) {
                    return_types.push_back(DetectYAMLType(it->second));
                } else {
                    return_types.push_back(LogicalType::VARCHAR);
                }
            }
        } else {
            // Single column for non-map nodes
            names.emplace_back("value");
            if (options.auto_detect_types) {
                return_types.emplace_back(DetectYAMLType(first_doc));
            } else {
                return_types.emplace_back(LogicalType::VARCHAR);
            }
        }
    }
    
    // Save column info
    result->names = names;
    result->types = return_types;
    
    return std::move(result);
}

} // namespace duckdb