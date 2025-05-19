#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/types/value.hpp"
#include "yaml-cpp/yaml.h"

namespace duckdb {

// Forward declaration
namespace yaml_utils {
std::string ValueToYAMLString(const Value& value, bool flow_format = false);
void EmitValueToYAML(YAML::Emitter& out, const Value& value);
}

//! Test helper functions for debugging the ValueToYAML segfault
class YAMLDebug {
public:
    //! Enable debugging mode for ValueToYAML function
    static void EnableDebugMode();
    
    //! Disable debugging mode for ValueToYAML function
    static void DisableDebugMode();
    
    //! Get the current debug mode status
    static bool IsDebugModeEnabled();
    
    //! Set a callback for debug logging
    static void SetDebugCallback(std::function<void(const std::string&)> callback);
    
    //! Log a debug message if debug mode is enabled
    static void LogDebug(const std::string& message);
    
    //! Safer version of EmitValueToYAML for debugging
    static bool SafeEmitValueToYAML(YAML::Emitter& out, const Value& value, int depth = 0);
    
    //! Safer version of ValueToYAMLString for debugging
    static std::string SafeValueToYAMLString(const Value& value, bool flow_format = false);

private:
    //! Flag indicating whether debug mode is enabled
    static bool debug_mode_enabled;
    
    //! Debug log callback function
    static std::function<void(const std::string&)> debug_callback;
    
    //! Maximum recursion depth for safe emission
    static constexpr int MAX_RECURSION_DEPTH = 100;
};

} // namespace duckdb