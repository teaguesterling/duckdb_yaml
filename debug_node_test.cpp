#include "yaml-cpp/yaml.h"
#include <iostream>

int main() {
    // Test: Create YAML::Node and emit with different formats
    YAML::Node node;
    node["name"] = "John";
    node["items"] = YAML::Node(YAML::NodeType::Sequence);
    node["items"].push_back(1);
    node["items"].push_back(2);
    node["items"].push_back(3);
    
    std::cout << "Node created successfully" << std::endl;
    
    // Test Flow
    std::cout << "\n=== FLOW FORMAT ===" << std::endl;
    YAML::Emitter flow_out;
    flow_out.SetIndent(2);
    flow_out.SetMapFormat(YAML::Flow);
    flow_out.SetSeqFormat(YAML::Flow);
    flow_out << node;
    std::cout << flow_out.c_str() << std::endl;
    
    // Test Block  
    std::cout << "\n=== BLOCK FORMAT ===" << std::endl;
    YAML::Emitter block_out;
    block_out.SetIndent(2);
    block_out.SetMapFormat(YAML::Block);
    block_out.SetSeqFormat(YAML::Block);
    block_out << node;
    std::cout << block_out.c_str() << std::endl;
    
    return 0;
}