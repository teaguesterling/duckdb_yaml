# YAML Extension Function Best Practices Review

## Current Function Analysis

### ✅ Good Practices We're Already Following

1. **Consistent Function Naming**
   - `ValueToYAMLFunction`, `FormatYAMLFunction`, `YAMLToJSONFunction`
   - Following DuckDB CamelCase convention

2. **Proper Error Handling**
   - Using appropriate exception types (`InvalidInputException`, `BinderException`)
   - Clear error messages with context

3. **Null Handling**
   - Explicit NULL checks in bind functions
   - Proper NULL propagation in execution

4. **Memory Management**
   - Using `make_uniq<>` for creating unique pointers
   - Proper RAII patterns

### 🔧 Areas for Improvement

#### 1. Error Message Consistency
**Current**: Mixed error message formats
**Improvement**: Standardize error messages with function name context

```cpp
// Before
throw InvalidInputException("format_yaml requires at least one argument");

// After
throw InvalidInputException("format_yaml requires at least one argument");
throw BinderException("format_yaml parameter '%s' must be a constant value", param_name);
throw InvalidInputException("Invalid YAML style '%s' for format_yaml. Valid options are 'flow' or 'block'.", style_str);
```

#### 2. Parameter Validation Consolidation
**Current**: Scattered validation logic
**Improvement**: Create helper functions for common validations

```cpp
// Helper function for parameter validation
static void ValidateYAMLStyleParameter(const string& style_str, const string& function_name) {
    string lower_style = StringUtil::Lower(style_str);
    if (lower_style != "block" && lower_style != "flow") {
        throw InvalidInputException("Invalid YAML style '%s' for %s. Valid options are 'flow' or 'block'.", 
                                   style_str, function_name);
    }
}
```

#### 3. Debug Function Optimization
**Current**: Simple boolean return functions create unnecessary data chunks
**Improvement**: Use constant vectors more efficiently

```cpp
// Current debug functions could be optimized
static void YAMLDebugIsEnabledFunction(DataChunk& args, ExpressionState& state, Vector& result) {
    result.SetVectorType(VectorType::CONSTANT_VECTOR);
    ConstantVector::GetData<bool>(result)[0] = YAMLDebug::IsDebugModeEnabled();
}

// Better: Handle all rows if needed, or ensure single-value optimization
```

#### 4. Function Registration Consistency
**Current**: Some functions use bind functions, others don't
**Improvement**: Consistent registration patterns

```cpp
// Ensure all complex functions have bind functions where appropriate
auto format_yaml_fun = ScalarFunction("format_yaml", {LogicalType::ANY}, LogicalType::VARCHAR, 
                                     FormatYAMLFunction, FormatYAMLBind);
format_yaml_fun.varargs = LogicalType::ANY;
format_yaml_fun.null_handling = FunctionNullHandling::SPECIAL_HANDLING;
```

### 📋 Recommended Improvements

#### 1. Add Function Documentation Headers
```cpp
//===--------------------------------------------------------------------===//
// format_yaml(data, style := 'flow'|'block', orient := 'document'|'sequence')
//===--------------------------------------------------------------------===//
// Converts DuckDB values to YAML string format with configurable parameters
// - data: Any DuckDB value to convert to YAML
// - style: YAML output style ('flow' for inline, 'block' for multi-line)  
// - orient: Top-level structure ('document' for single doc, 'sequence' for array)
// Returns: VARCHAR containing formatted YAML
//===--------------------------------------------------------------------===//
```

#### 2. Create Parameter Validation Utilities
```cpp
namespace yaml_utils {
    static void ValidateStyleParameter(const string& style_str, const string& function_name);
    static void ValidateOrientParameter(const string& orient_str, const string& function_name);
    static void ValidateQuoteParameter(const string& quote_str, const string& function_name);
}
```

#### 3. Standardize Function Return Types
- Use VARCHAR for string outputs (✅ already doing)
- Use appropriate logical types for structured data
- Consider using YAML type where semantically appropriate

#### 4. Improve Named Parameter Support
Following the pattern discovered in struct_update:
- Validate named parameters in bind function
- Access parameters correctly in execution function
- Support both named and positional parameter styles

#### 5. Add Parameter Default Handling
```cpp
// In bind function
auto bind_data = make_uniq<FormatYAMLBindData>();
bind_data->style = yaml_utils::YAMLSettings::GetDefaultFormat();
bind_data->orient = "document";  // Default value
bind_data->indent = 2;           // Default value
bind_data->quote = "auto";       // Default value
```

### 🎯 Next Steps

1. **Immediate**: Fix named parameter pattern in FormatYAMLFunction
2. **Short-term**: Add parameter validation utilities
3. **Medium-term**: Implement orient, indent, quote parameters
4. **Long-term**: Add comprehensive function documentation

### 📝 Function Signature Goals

Target final signatures:
```sql
-- Core functions
SELECT value_to_yaml(data) -> YAML;
SELECT format_yaml(data, style := 'block', orient := 'document', indent := 4) -> VARCHAR;

-- Conversion functions  
SELECT yaml_to_json(yaml_data) -> JSON;
SELECT json_to_yaml(json_data) -> YAML;

-- Configuration functions
SELECT yaml_set_default_style('block') -> VARCHAR;
SELECT yaml_get_default_style() -> VARCHAR;

-- Debug functions
SELECT yaml_debug_enable() -> BOOLEAN;
SELECT yaml_debug_value_to_yaml(data) -> VARCHAR;
```