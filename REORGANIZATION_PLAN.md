# YAML Extension Reorganization Plan

## Current Issues

1. **yaml_types.cpp is too large** - Contains utilities, type system, scalar functions, and COPY TO logic
2. **CopyFormatYAMLFunction is in the wrong file** - Should be with other COPY logic
3. **Layout transformation logic is duplicated** - Present in both yaml_copy.cpp and yaml_types.cpp
4. **yaml_utils namespace could be its own file** - Better separation of concerns
5. **Dead code exists** - PostProcessYAMLForLayout in yaml_copy.cpp is unused

## Proposed File Structure

### 1. yaml_utils.cpp/hpp (NEW)
Move all utility functions from yaml_types.cpp:
- `YAMLSettings` class
- `ConfigureEmitter()`
- `EmitYAML()`
- `EmitYAMLMultiDoc()`
- `ParseYAML()`
- `YAMLNodeToJSON()`
- `ValueToYAMLNode()`
- `EmitValueToYAML()`
- `ValueToYAMLString()`
- `FormatPerStyleAndLayout()` (simplified - no layout handling)

### 2. yaml_formatting.cpp/hpp (NEW)
YAML formatting and layout logic:
- Layout transformation functions (sequence/document formatting)
- Style configuration (block/flow)
- Document separator handling
- Indentation logic

### 3. yaml_types.cpp (REFACTORED)
Keep only type-related functionality:
- `YAMLType()` function
- Cast functions (YAML<->JSON, YAML<->VARCHAR)
- Type registration

### 4. yaml_scalar_functions.cpp (RENAME from yaml_functions.cpp)
Move scalar functions from yaml_types.cpp:
- `value_to_yaml()`
- `format_yaml()`
- `yaml_to_json()`
- Default style management functions
- Keep existing scalar functions already there

### 5. yaml_copy.cpp (ENHANCED)
Consolidate all COPY TO logic:
- Move `CopyFormatYAMLFunction` from yaml_types.cpp
- Remove dead `PostProcessYAMLForLayout` function
- Use yaml_formatting utilities for layout handling

### 6. yaml_debug.cpp (KEEP AS IS)
Already well-organized with debug functionality

## Implementation Steps

1. **Create yaml_utils.cpp/hpp**
   - Extract utility functions from yaml_types.cpp
   - Create proper header with namespace yaml_utils
   - Update all references

2. **Create yaml_formatting.cpp/hpp**
   - Extract layout/formatting logic
   - Create unified formatting API
   - Handle sequence prefixing and document separators

3. **Move CopyFormatYAMLFunction**
   - Move from yaml_types.cpp to yaml_copy.cpp
   - Update to use yaml_formatting utilities
   - Remove duplicated layout logic

4. **Refactor yaml_types.cpp**
   - Keep only type system code
   - Move scalar functions to yaml_functions.cpp
   - Clean up includes

5. **Clean up yaml_copy.cpp**
   - Remove dead PostProcessYAMLForLayout
   - Use yaml_formatting utilities
   - Simplify the implementation

6. **Update includes and CMakeLists.txt**
   - Add new files to build
   - Update all #include statements
   - Ensure proper dependency order

## Benefits

1. **Better separation of concerns** - Each file has a clear purpose
2. **Reduced code duplication** - Layout logic in one place
3. **Easier maintenance** - Smaller, focused files
4. **Clearer dependencies** - Utilities separated from business logic
5. **More testable** - Isolated components easier to unit test

## Testing Strategy

1. Run full test suite after each major step
2. Ensure no functionality is lost
3. Verify COPY TO still works with all layout/style combinations
4. Check that all scalar functions still work
5. Ensure type conversions remain intact