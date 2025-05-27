# DuckDB Named Parameters Pattern for Scalar Functions

## Overview

This document describes the correct pattern for implementing named parameters in DuckDB scalar functions, based on analysis of the `struct_update` function and similar implementations.

## Key Components

### 1. Function Registration
```cpp
ScalarFunction fun({}, LogicalTypeId::STRUCT, ExecutionFunction, BindFunction);
fun.varargs = LogicalType::ANY;  // Allow variable arguments
```

### 2. Bind Function Pattern
```cpp
static unique_ptr<FunctionData> BindFunction(ClientContext &context, ScalarFunction &bound_function,
                                           vector<unique_ptr<Expression>> &arguments) {
    // Validate first argument
    if (arguments.empty()) {
        throw InvalidInputException("Missing required arguments");
    }
    
    // Process named parameters
    for (idx_t arg_idx = 1; arg_idx < arguments.size(); arg_idx++) {
        auto &child = arguments[arg_idx];
        
        if (!child->alias.empty()) {
            // This is a named parameter using := syntax
            string param_name = StringUtil::Lower(child->alias);
            
            // Validate parameter value is constant
            if (child->type != ExpressionType::VALUE_CONSTANT) {
                throw BinderException("Parameter '%s' must be a constant value", param_name);
            }
            
            auto &constant = child->Cast<ConstantExpression>();
            if (constant.value.IsNull()) {
                throw BinderException("Parameter '%s' cannot be NULL", param_name);
            }
            
            // Process specific parameters
            if (param_name == "param1") {
                // Handle param1
            } else if (param_name == "param2") {
                // Handle param2
            } else {
                throw BinderException("Unknown parameter '%s'", param_name);
            }
        } else {
            // Unnamed parameter - could be positional or struct parameter
            // Handle based on your function's needs
        }
    }
    
    return make_uniq<YourFunctionData>();
}
```

### 3. Execution Function Pattern
```cpp
static void ExecutionFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &input = args.data[0];
    
    // Access named parameters from bound function expression
    auto &func_args = state.expr.Cast<BoundFunctionExpression>().children;
    
    // Process named parameters at runtime
    for (idx_t arg_idx = 1; arg_idx < func_args.size(); arg_idx++) {
        auto &child = func_args[arg_idx];
        
        if (!child->alias.empty()) {
            string param_name = StringUtil::Lower(child->alias);
            
            if (param_name == "param1") {
                // Get value from the corresponding data chunk argument
                Value param_value = args.data[arg_idx].GetValue(0);
                // Process the parameter
            }
        }
    }
    
    // Execute main function logic
    for (idx_t i = 0; i < args.size(); i++) {
        // Process each row
    }
}
```

## Important Notes

### Accessing Parameters
- **Bind Time**: Use `arguments[i]->alias` and `arguments[i]->Cast<ConstantExpression>().value`
- **Runtime**: Use `state.expr.Cast<BoundFunctionExpression>().children[i]->alias` and `args.data[i].GetValue(0)`

### Parameter Validation
- Named parameters should be validated as constants in the bind function
- NULL parameter values should be rejected or handled explicitly
- Unknown parameter names should throw clear error messages

### Mixed Parameter Support
Functions can support both:
- Named parameters: `func(data, param1 := value1, param2 := value2)`
- Struct parameters: `func(data, {'param1': value1})` (for COPY TO integration)
- Positional parameters: `func(data, value1, value2)`

## Best Practices

### Error Messages
```cpp
// Good error messages
throw BinderException("Need named argument for %s, e.g., param := value", function_name);
throw InvalidInputException("Invalid value '%s' for parameter '%s'. Valid options are: %s", 
                           value, param_name, valid_options);
```

### Parameter Processing
- Use `StringUtil::Lower()` for case-insensitive parameter names
- Validate enum-like parameters with explicit value checking
- Provide clear error messages with valid options

### Function Registration
- Set `fun.varargs = LogicalType::ANY` for functions accepting named parameters
- Consider null handling: `fun.null_handling = FunctionNullHandling::SPECIAL_HANDLING`

## Common Pitfalls

1. **Don't access bind_info in execution function** - Use `state.expr.Cast<BoundFunctionExpression>().children` instead
2. **Include proper headers** - Need `#include "duckdb/planner/expression/bound_function_expression.hpp"`
3. **Handle both named and unnamed parameters** - Check `child->alias.empty()` in both bind and execution
4. **Validate parameter types** - Named parameters should typically be constants

## Integration with COPY TO

Functions supporting named parameters can also work with COPY TO by:
1. Accepting struct parameters as an alternative to named parameters
2. Processing struct fields at runtime in the execution function
3. Using the same parameter names for both approaches

Example:
```sql
-- Named parameter syntax
SELECT format_yaml(data, style := 'block')

-- COPY TO syntax (generates struct parameter)
COPY (...) TO 'file.yaml' (FORMAT yaml, STYLE block)
```