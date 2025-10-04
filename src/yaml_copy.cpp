#include "duckdb/function/copy_function.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/expression/positional_reference_expression.hpp"
#include "duckdb/parser/query_node/select_node.hpp"
#include "duckdb/parser/tableref/subqueryref.hpp"
#include "duckdb/planner/binder.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "yaml_types.hpp"
#include "yaml_utils.hpp"
#include "yaml_formatting.hpp"

namespace duckdb {


static void ThrowYAMLCopyParameterException(const string &loption) {
    throw BinderException("COPY (FORMAT YAML) parameter %s expects a single argument.", loption);
}

static BoundStatement CopyToYAMLPlan(Binder &binder, CopyStatement &stmt) {
    auto stmt_copy = stmt.Copy();
    auto &copy = stmt_copy->Cast<CopyStatement>();
    auto &copied_info = *copy.info;

    // Parse YAML-specific options, creating options for the CSV writer
    string yaml_style = ""; // Default to empty, will use global default
    string yaml_layout = ""; // Default to empty, will infer from style
    case_insensitive_map_t<vector<Value>> csv_copy_options {{"file_extension", {"yaml"}}};
    
    for (const auto &kv : copied_info.options) {
        const auto &loption = StringUtil::Lower(kv.first);
        if (loption == "style") {
            if (kv.second.size() != 1) {
                ThrowYAMLCopyParameterException(loption);
            }
            yaml_style = StringValue::Get(kv.second.back());
            auto lowercase_style = StringUtil::Lower(yaml_style);
            if (lowercase_style != "block" && lowercase_style != "flow") {
                throw BinderException("Invalid YAML style '%s'. Valid options are 'flow' or 'block'.", yaml_style.c_str());
            }
        } else if (loption == "layout") {
            if (kv.second.size() != 1) {
                ThrowYAMLCopyParameterException(loption);
            }
            yaml_layout = StringValue::Get(kv.second.back());
            auto lowercase_layout = StringUtil::Lower(yaml_layout);
            if (lowercase_layout != "sequence" && lowercase_layout != "document") {
                throw BinderException("Invalid YAML layout '%s'. Valid options are 'sequence' or 'document'.", yaml_layout.c_str());
            }
        } else if (loption == "compression" || loption == "encoding" || loption == "per_thread_output" ||
                   loption == "file_size_bytes" || loption == "use_tmp_file" || loption == "overwrite_or_ignore" ||
                   loption == "filename_pattern" || loption == "file_extension") {
            // We support these base options
            csv_copy_options.insert(kv);
        } else {
            throw BinderException("Unknown option for COPY ... TO ... (FORMAT YAML): \"%s\".", loption);
        }
    }

    // Bind the select statement of the original to resolve the types
    auto dummy_binder = Binder::CreateBinder(binder.context, &binder);
    auto bound_original = dummy_binder->Bind(*stmt.info->select_statement);

    // Create new SelectNode with the original SelectNode as a subquery in the FROM clause
    auto select_stmt = make_uniq<SelectStatement>();
    select_stmt->node = std::move(copied_info.select_statement);
    auto subquery_ref = make_uniq<SubqueryRef>(std::move(select_stmt));

    copied_info.select_statement = make_uniq_base<QueryNode, SelectNode>();
    auto &select_node = copied_info.select_statement->Cast<SelectNode>();
    select_node.from_table = std::move(subquery_ref);

    // Create new select list - each column becomes a positional reference with alias
    vector<unique_ptr<ParsedExpression>> select_list;
    select_list.reserve(bound_original.types.size());

    for (idx_t col_idx = 0; col_idx < bound_original.types.size(); col_idx++) {
        const auto &name = bound_original.names[col_idx];

        // Create positional reference and set alias (needed for struct_pack named fields)
        auto column = make_uniq_base<ParsedExpression, PositionalReferenceExpression>(col_idx + 1);
        column->SetAlias(name);
        select_list.emplace_back(std::move(column));
    }

    // Now create the struct_pack to create a struct with all columns
    // struct_pack uses the aliases automatically as field names
    auto struct_pack_expr = make_uniq<FunctionExpression>("struct_pack", std::move(select_list));

    // Now convert to YAML using format_yaml
    vector<unique_ptr<ParsedExpression>> format_yaml_children;
    format_yaml_children.emplace_back(std::move(struct_pack_expr));
    
    // Add style parameter if specified (using named parameter syntax)
    if (!yaml_style.empty()) {
        // Create named parameter: style := 'block' or style := 'flow'
        auto style_value = make_uniq<ConstantExpression>(yaml_style);
        style_value->SetAlias("style");  // This creates the named parameter
        format_yaml_children.emplace_back(std::move(style_value));
    }

    // For COPY TO, always use layout=document internally to avoid array wrapping
    // The actual layout transformation will be handled via post-processing
    auto internal_layout_value = make_uniq<ConstantExpression>("document");
    internal_layout_value->SetAlias("layout");
    format_yaml_children.emplace_back(std::move(internal_layout_value));

    // Add the target layout and style as additional parameters for post-processing
    if (!yaml_layout.empty()) {
        auto target_layout_value = make_uniq<ConstantExpression>(yaml_layout);
        format_yaml_children.emplace_back(std::move(target_layout_value));
    } else {
        auto target_layout_value = make_uniq<ConstantExpression>("document");  // Default
        format_yaml_children.emplace_back(std::move(target_layout_value));
    }

    if (!yaml_style.empty()) {
        auto target_style_value = make_uniq<ConstantExpression>(yaml_style);
        format_yaml_children.emplace_back(std::move(target_style_value));
    } else {
        auto target_style_value = make_uniq<ConstantExpression>("flow");  // Default
        format_yaml_children.emplace_back(std::move(target_style_value));
    }

    // Create copy_format_yaml function call (we'll register this function to handle post-processing)
    select_node.select_list.emplace_back(make_uniq<FunctionExpression>("copy_format_yaml", std::move(format_yaml_children)));

    // Now we can just use the CSV writer with YAML content
    copied_info.format = "csv";
    copied_info.options = std::move(csv_copy_options);
    copied_info.options["quote"] = {""};
    copied_info.options["escape"] = {""};
    copied_info.options["delimiter"] = {"\n"};
    copied_info.options["header"] = {{0}};

    return binder.Bind(*stmt_copy);
}

//===--------------------------------------------------------------------===//
// COPY Format YAML Function
//===--------------------------------------------------------------------===//

static void CopyFormatYAMLFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    auto& input = args.data[0];

    // Get target layout and style from the last two arguments
    Value target_layout_value = args.data[args.ColumnCount() - 2].GetValue(0);
    Value target_style_value = args.data[args.ColumnCount() - 1].GetValue(0);

    string target_layout_str = StringUtil::Lower(target_layout_value.ToString());
    string target_style = StringUtil::Lower(target_style_value.ToString());

    // Convert layout string to enum
    yaml_formatting::YAMLLayout layout = yaml_formatting::YAMLLayout::DOCUMENT;
    if (target_layout_str == "sequence") {
        layout = yaml_formatting::YAMLLayout::SEQUENCE;
    }

    // Determine YAML format from target style
    yaml_utils::YAMLFormat format = yaml_utils::YAMLSettings::GetDefaultFormat();
    if (target_style == "block") {
        format = yaml_utils::YAMLFormat::BLOCK;
    } else if (target_style == "flow") {
        format = yaml_utils::YAMLFormat::FLOW;
    }

    // Process each row with post-processing
    for (idx_t row_idx = 0; row_idx < args.size(); row_idx++) {
        Value value = input.GetValue(row_idx);

        try {
            // Get the base YAML string
            std::string yaml_str = yaml_utils::ValueToYAMLString(value, format);

            // Apply layout-specific formatting
            yaml_str = yaml_formatting::PostProcessForLayout(yaml_str, layout, format, row_idx);

            result.SetValue(row_idx, Value(yaml_str));
        } catch (const std::exception& e) {
            result.SetValue(row_idx, Value("null"));
        } catch (...) {
            result.SetValue(row_idx, Value("null"));
        }
    }
}

void RegisterYAMLCopyFunctions(ExtensionLoader &loader) {
    // Register the COPY TO function
    auto copy_function = CopyFunction("yaml");
    copy_function.extension = "yaml";
    copy_function.plan = CopyToYAMLPlan;
    loader.RegisterFunction(copy_function);

    // Register copy_format_yaml function for COPY TO post-processing
    auto copy_format_yaml_fun = ScalarFunction("copy_format_yaml", {LogicalType::ANY}, LogicalType::VARCHAR, CopyFormatYAMLFunction);
    copy_format_yaml_fun.null_handling = FunctionNullHandling::SPECIAL_HANDLING;
    copy_format_yaml_fun.varargs = LogicalType::ANY;  // Allow variable number of arguments
    loader.RegisterFunction(copy_format_yaml_fun);
}

CopyFunction GetYAMLCopyFunction() {
    CopyFunction function("yaml");
    function.extension = "yaml";

    function.plan = CopyToYAMLPlan;

    // Note: We don't implement copy_from_bind or copy_from_function here 
    // because YAML reading is handled by read_yaml table functions

    return function;
}

} // namespace duckdb
