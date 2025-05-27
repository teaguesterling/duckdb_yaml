// Clean version of FormatYAMLFunction following struct_update pattern exactly
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
        // TODO: Add support for other parameters like orient, indent, quote
    }

    // Process each row
    for (idx_t i = 0; i < args.size(); i++) {
        // Extract the value from the input vector
        Value value = input.GetValue(i);

        // YAML generation using determined parameters
        try {
            if (YAMLDebug::IsDebugModeEnabled()) {
                std::string yaml_str = YAMLDebug::SafeValueToYAMLString(value, format == yaml_utils::YAMLFormat::BLOCK);
                result.SetValue(i, Value(yaml_str));
            } else {
                // Use the regular version with specified parameters
                std::string yaml_str = yaml_utils::ValueToYAMLString(value, format);
                result.SetValue(i, Value(yaml_str));
            }
        } catch (const std::exception& e) {
            // Only catch YAML generation errors, return null for data conversion issues
            result.SetValue(i, Value("null"));
        } catch (...) {
            // For unknown YAML generation exceptions, return null
            result.SetValue(i, Value("null"));
        }
    }
}