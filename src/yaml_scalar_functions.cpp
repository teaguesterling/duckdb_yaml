#include "yaml_scalar_functions.hpp"
#include "duckdb/main/client_context.hpp"
#include "yaml_types.hpp"
#include "yaml_utils.hpp"
#include "yaml_reader.hpp"
#include "yaml-cpp/yaml.h"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/function/scalar/nested_functions.hpp"
#include <algorithm>
#include <cctype>

namespace duckdb {

void YAMLFunctions::Register(ExtensionLoader &loader) {
    // Register validation and basic functions
    RegisterValidationFunction(loader);
    RegisterYAMLTypeFunctions(loader);
    RegisterStyleFunctions(loader);
    RegisterFromYAMLFunction(loader);
}


void YAMLFunctions::RegisterValidationFunction(ExtensionLoader &loader) {
    auto yaml_type = YAMLTypes::YAMLType();
    
    // Basic YAML validity check for VARCHAR
    ScalarFunction yaml_valid_varchar("yaml_valid", {LogicalType::VARCHAR}, LogicalType::BOOLEAN, 
        [](DataChunk &args, ExpressionState &state, Vector &result) {
            auto &input_vector = args.data[0];
            
            UnaryExecutor::ExecuteWithNulls<string_t, bool>(input_vector, result, args.size(),
                [&](string_t yaml_str, ValidityMask &mask, idx_t idx) {
                    if (!mask.RowIsValid(idx)) {
                        return false;
                    }
                    
                    try {
                        // Check if it's a multi-document YAML by trying to load all documents
                        std::stringstream yaml_stream(yaml_str.GetString());
                        std::vector<YAML::Node> docs = YAML::LoadAll(yaml_stream);
                        return !docs.empty();
                    } catch (...) {
                        return false;
                    }
                });
        });
    
    // YAML validity check for YAML type (always returns true for valid YAML objects)
    ScalarFunction yaml_valid_yaml("yaml_valid", {yaml_type}, LogicalType::BOOLEAN, 
        [](DataChunk &args, ExpressionState &state, Vector &result) {
            // YAML types are already validated, so they're always valid
            auto &input_vector = args.data[0];
            UnaryExecutor::ExecuteWithNulls<string_t, bool>(input_vector, result, args.size(),
                [&](string_t yaml_str, ValidityMask &mask, idx_t idx) {
                    return mask.RowIsValid(idx); // Return true if not NULL
                });
        });
    
    loader.RegisterFunction(yaml_valid_varchar);
    loader.RegisterFunction(yaml_valid_yaml);
}

//===--------------------------------------------------------------------===//
// YAML Type Functions
//===--------------------------------------------------------------------===//

static void YAMLToJSONFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    UnaryExecutor::Execute<string_t, string_t>(
        args.data[0], result, args.size(),
        [&](string_t yaml_str) -> string_t {
            if (yaml_str.GetSize() == 0) {
                return string_t();
            }
            
            try {
                // Parse as multi-document YAML
                const auto docs = yaml_utils::ParseYAML(yaml_str.GetString(), true);
                
                std::string json_str;
                if (docs.empty()) {
                    json_str = "null";
                } else if (docs.size() == 1) {
                    // Single document - convert directly to JSON
                    json_str = yaml_utils::YAMLNodeToJSON(docs[0]);
                } else {
                    // Multiple documents - convert to JSON array
                    json_str = "[";
                    for (idx_t doc_idx = 0; doc_idx < docs.size(); doc_idx++) {
                        if (doc_idx > 0) {
                            json_str += ",";
                        }
                        json_str += yaml_utils::YAMLNodeToJSON(docs[doc_idx]);
                    }
                    json_str += "]";
                }
                
                return StringVector::AddString(result, json_str.c_str(), json_str.length());
            } catch (const std::exception& e) {
                throw InvalidInputException("Error converting YAML to JSON: %s", e.what());
            }
        });
}

static void ValueToYAMLFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    auto& input = args.data[0];

    // Process each row
    for (idx_t row_idx = 0; row_idx < args.size(); row_idx++) {
        try {
            // Extract the value from the input vector
            Value value = input.GetValue(row_idx);

            // Use the regular version with flow format for consistent YAML objects
            std::string yaml_str = yaml_utils::ValueToYAMLString(value, yaml_utils::YAMLFormat::FLOW);
            result.SetValue(row_idx, Value(yaml_str));
        } catch (const std::exception& e) {
            // If there's a known exception, return a descriptive message
            result.SetValue(row_idx, Value("null"));
        } catch (...) {
            // For unknown exceptions, just return null
            result.SetValue(row_idx, Value("null"));
        }
    }
}

//===--------------------------------------------------------------------===//
// YAML Function Bind Data
//===--------------------------------------------------------------------===//

struct FormatYAMLBindData : public FunctionData {
    yaml_utils::YAMLFormat style = yaml_utils::YAMLFormat::FLOW;
    string orient = "document";  // document/sequence/objects/mapping
    idx_t indent = 2;
    string quote = "auto";       // auto/always/never/single/double

    unique_ptr<FunctionData> Copy() const override {
        auto result = make_uniq<FormatYAMLBindData>();
        result->style = style;
        result->orient = orient;
        result->indent = indent;
        result->quote = quote;
        return std::move(result);
    }

    bool Equals(const FunctionData &other_p) const override {
        auto &other = other_p.Cast<FormatYAMLBindData>();
        return style == other.style && orient == other.orient &&
               indent == other.indent && quote == other.quote;
    }
};

static unique_ptr<FunctionData> FormatYAMLBind(ClientContext &context, ScalarFunction &bound_function,
                                              vector<unique_ptr<Expression>> &arguments) {

    if (arguments.empty()) {
        throw InvalidInputException("format_yaml requires at least one argument");
    }

    // Just validate parameters, don't store custom bind data (following struct_update pattern)

    // Process parameters (arguments beyond the first one)
    for (idx_t param_idx = 1; param_idx < arguments.size(); param_idx++) {
        auto &child = arguments[param_idx];
        string param_name = StringUtil::Lower(child->alias);


        // Check if this is a named parameter (using := syntax)
        if (param_name.empty()) {
            throw BinderException("Need named argument for format_yaml, e.g. style := 'block'");
        }

        // Validate parameter value is constant (following DuckDB pattern)
        if (child->type != ExpressionType::VALUE_CONSTANT) {
            throw BinderException("format_yaml parameter '%s' must be a constant value", param_name);
        }

        // Validate known parameter names
        if (param_name == "style") {
            // Valid parameter, will be processed in execution function
        } else {
            throw BinderException("Unknown parameter '%s' for format_yaml", param_name);
        }
    }

    // Return simple bind data like struct_update does
    return make_uniq<VariableReturnBindData>(LogicalType::VARCHAR);
}

static void FormatYAMLFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    auto& input = args.data[0];
    yaml_utils::YAMLFormat format = yaml_utils::YAMLSettings::GetDefaultFormat();

    // Access named parameters from the bound function expression (like struct_update)
    auto &func_args = state.expr.Cast<BoundFunctionExpression>().children;

    // Process named parameters - all parameters beyond first must be named (like struct_update)
    for (idx_t arg_idx = 1; arg_idx < func_args.size(); arg_idx++) {
        auto &child = func_args[arg_idx];
        if (child->alias.empty()) {
            throw InvalidInputException("format_yaml requires named parameters, e.g. style := 'block'");
        }

        string param_name = StringUtil::Lower(child->alias);

        if (param_name == "style") {
            // Get the style value from the corresponding argument
            Value style_value = args.data[arg_idx].GetValue(0);
            string style_str = StringUtil::Lower(style_value.ToString());
            if (style_str == "block") {
                format = yaml_utils::YAMLFormat::BLOCK;
            } else if (style_str == "flow") {
                format = yaml_utils::YAMLFormat::FLOW;
            } else {
                throw InvalidInputException("Invalid YAML style '%s'. Valid options are 'flow' or 'block'.", style_str);
            }
        } else {
            throw InvalidInputException("Unknown parameter '%s' for format_yaml", param_name);
        }
    }

    // Process each row
    for (idx_t row_idx = 0; row_idx < args.size(); row_idx++) {
        // Extract the value from the input vector
        Value value = input.GetValue(row_idx);

        // YAML generation using determined parameters
        try {
            // Format the value as YAML with the specified style
            std::string yaml_str = yaml_utils::ValueToYAMLString(value, format);
            result.SetValue(row_idx, Value(yaml_str));
        } catch (const std::exception& e) {
            // Only catch YAML generation errors, return null for data conversion issues
            result.SetValue(row_idx, Value("null"));
        } catch (...) {
            // For unknown YAML generation exceptions, return null
            result.SetValue(row_idx, Value("null"));
        }
    }
}

void YAMLFunctions::RegisterYAMLTypeFunctions(ExtensionLoader &loader) {
    // Get the YAML type
    auto yaml_type = YAMLTypes::YAMLType();
    
    // Register yaml_to_json function
    auto yaml_to_json_fun = ScalarFunction("yaml_to_json", {yaml_type}, LogicalType::JSON(), YAMLToJSONFunction);
    loader.RegisterFunction(yaml_to_json_fun);
    
    // Register value_to_yaml function (single parameter, returns YAML type)
    auto value_to_yaml_fun = ScalarFunction("value_to_yaml", {LogicalType::ANY}, yaml_type, ValueToYAMLFunction);
    loader.RegisterFunction(value_to_yaml_fun);

    // Register format_yaml function with named parameters (returns VARCHAR for display/formatting)
    auto format_yaml_fun = ScalarFunction("format_yaml", {LogicalType::ANY}, LogicalType::VARCHAR, FormatYAMLFunction, FormatYAMLBind);
    format_yaml_fun.null_handling = FunctionNullHandling::SPECIAL_HANDLING;
    format_yaml_fun.varargs = LogicalType::ANY;  // Allow variable number of arguments
    loader.RegisterFunction(format_yaml_fun);
    
    // Register yaml() constructor function (parses YAML string to YAML type)
    auto yaml_constructor_fun = ScalarFunction("yaml", {LogicalType::VARCHAR}, yaml_type, 
        [](DataChunk &args, ExpressionState &state, Vector &result) {
            UnaryExecutor::Execute<string_t, string_t>(
                args.data[0], result, args.size(),
                [&](string_t input) {
                    string input_str = input.GetString();
                    // Validate that it's valid YAML by parsing it
                    try {
                        YAML::Load(input_str);
                        // If parsing succeeds, return the input as a YAML value
                        return StringVector::AddString(result, input_str);
                    } catch (const YAML::Exception &e) {
                        throw InvalidInputException("Invalid YAML: %s", e.what());
                    }
                });
        });
    loader.RegisterFunction(yaml_constructor_fun);
}


//===--------------------------------------------------------------------===//
// Style Management Functions
//===--------------------------------------------------------------------===//

static void YAMLSetDefaultStyleFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    auto& input = args.data[0];

    for (idx_t row_idx = 0; row_idx < args.size(); row_idx++) {
        Value style_value = input.GetValue(row_idx);

        if (style_value.IsNull()) {
            throw InvalidInputException("YAML style cannot be NULL");
        }

        auto style_str = style_value.ToString();
        std::transform(style_str.begin(), style_str.end(), style_str.begin(), ::tolower);

        if (style_str == "block") {
            yaml_utils::YAMLSettings::SetDefaultFormat(yaml_utils::YAMLFormat::BLOCK);
        } else if (style_str == "flow") {
            yaml_utils::YAMLSettings::SetDefaultFormat(yaml_utils::YAMLFormat::FLOW);
        } else {
            throw InvalidInputException("Invalid YAML style '%s'. Valid options are 'flow' or 'block'.", style_str.c_str());
        }

        result.SetValue(row_idx, Value(style_str));
    }
}

static void YAMLGetDefaultStyleFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    auto current_format = yaml_utils::YAMLSettings::GetDefaultFormat();
    std::string format_name = (current_format == yaml_utils::YAMLFormat::BLOCK) ? "block" : "flow";

    result.SetVectorType(VectorType::CONSTANT_VECTOR);
    auto result_data = ConstantVector::GetData<string_t>(result);
    result_data[0] = StringVector::AddString(result, format_name.c_str(), format_name.length());
}

void YAMLFunctions::RegisterStyleFunctions(ExtensionLoader &loader) {
    // Register default style management functions
    auto yaml_set_default_style_fun = ScalarFunction("yaml_set_default_style", {LogicalType::VARCHAR}, LogicalType::VARCHAR, YAMLSetDefaultStyleFunction);
    loader.RegisterFunction(yaml_set_default_style_fun);

    auto yaml_get_default_style_fun = ScalarFunction("yaml_get_default_style", {}, LogicalType::VARCHAR, YAMLGetDefaultStyleFunction);
    loader.RegisterFunction(yaml_get_default_style_fun);
}

//===--------------------------------------------------------------------===//
// from_yaml Function - Convert YAML to structured types
//===--------------------------------------------------------------------===//

struct FromYAMLBindData : public FunctionData {
    LogicalType target_type;

    unique_ptr<FunctionData> Copy() const override {
        auto result = make_uniq<FromYAMLBindData>();
        result->target_type = target_type;
        return std::move(result);
    }

    bool Equals(const FunctionData &other_p) const override {
        auto &other = other_p.Cast<FromYAMLBindData>();
        return target_type == other.target_type;
    }
};

static unique_ptr<FunctionData> FromYAMLBind(ClientContext &context, ScalarFunction &bound_function,
                                              vector<unique_ptr<Expression>> &arguments) {
    if (arguments.size() != 2) {
        throw InvalidInputException("from_yaml requires exactly 2 arguments: yaml_value and structure");
    }

    auto bind_data = make_uniq<FromYAMLBindData>();

    // The second argument should be a constant structure specification
    if (arguments[1]->return_type.id() == LogicalTypeId::STRUCT) {
        // If it's a struct type, use it directly
        bind_data->target_type = arguments[1]->return_type;
    } else if (arguments[1]->return_type.id() == LogicalTypeId::LIST) {
        // If it's a list type, use it directly
        bind_data->target_type = arguments[1]->return_type;
    } else if (arguments[1]->return_type.id() == LogicalTypeId::SQLNULL) {
        // NULL type defaults to VARCHAR
        bind_data->target_type = LogicalType::VARCHAR;
    } else {
        // For scalar types, use the type directly
        bind_data->target_type = arguments[1]->return_type;
    }

    // Set the return type to the target type
    bound_function.return_type = bind_data->target_type;

    return std::move(bind_data);
}

static void FromYAMLFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &func_expr = state.expr.Cast<BoundFunctionExpression>();
    auto &bind_data = func_expr.bind_info->Cast<FromYAMLBindData>();
    auto &target_type = bind_data.target_type;

    auto &yaml_input = args.data[0];

    // Process each row
    for (idx_t row_idx = 0; row_idx < args.size(); row_idx++) {
        Value yaml_value = yaml_input.GetValue(row_idx);

        if (yaml_value.IsNull()) {
            result.SetValue(row_idx, Value(target_type));
            continue;
        }

        try {
            // Get YAML string and parse it
            std::string yaml_str = yaml_value.ToString();
            YAML::Node node = YAML::Load(yaml_str);

            // Convert to the target type using existing conversion function
            Value converted = YAMLReader::YAMLNodeToValue(node, target_type);
            result.SetValue(row_idx, converted);
        } catch (const std::exception &e) {
            throw InvalidInputException("Error converting YAML to type '%s': %s",
                                       target_type.ToString(), e.what());
        }
    }
}

void YAMLFunctions::RegisterFromYAMLFunction(ExtensionLoader &loader) {
    auto yaml_type = YAMLTypes::YAMLType();

    // Register from_yaml function that takes YAML and structure spec
    // from_yaml(yaml_value, structure) -> structured type
    auto from_yaml_fun = ScalarFunction("from_yaml", {yaml_type, LogicalType::ANY}, LogicalType::ANY,
                                        FromYAMLFunction, FromYAMLBind);
    from_yaml_fun.null_handling = FunctionNullHandling::SPECIAL_HANDLING;
    loader.RegisterFunction(from_yaml_fun);

    // Also register version that takes VARCHAR input
    auto from_yaml_varchar_fun = ScalarFunction("from_yaml", {LogicalType::VARCHAR, LogicalType::ANY},
                                                LogicalType::ANY, FromYAMLFunction, FromYAMLBind);
    from_yaml_varchar_fun.null_handling = FunctionNullHandling::SPECIAL_HANDLING;
    loader.RegisterFunction(from_yaml_varchar_fun);
}

} // namespace duckdb
