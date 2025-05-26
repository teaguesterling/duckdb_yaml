#include "yaml-cpp/yaml.h"
#include <iostream>
#include <string>

void test_emitter_configuration() {
    std::cout << "=== Testing YAML-CPP Emitter Configuration ===" << std::endl;
    
    // Create a simple structure
    YAML::Node node;
    node["name"] = "John";
    node["items"] = YAML::Node(YAML::NodeType::Sequence);
    node["items"].push_back(1);
    node["items"].push_back(2);
    node["items"].push_back(3);
    
    std::cout << "\n--- Flow Format ---" << std::endl;
    {
        YAML::Emitter out;
        out.SetIndent(2);
        out.SetMapFormat(YAML::Flow);
        out.SetSeqFormat(YAML::Flow);
        out << node;
        std::cout << out.c_str() << std::endl;
    }
    
    std::cout << "\n--- Block Format ---" << std::endl;
    {
        YAML::Emitter out;
        out.SetIndent(2);
        out.SetMapFormat(YAML::Block);
        out.SetSeqFormat(YAML::Block);
        out << node;
        std::cout << out.c_str() << std::endl;
    }
    
    std::cout << "\n--- Manual Building with Flow Format ---" << std::endl;
    {
        YAML::Emitter out;
        out.SetIndent(2);
        out.SetMapFormat(YAML::Flow);
        out.SetSeqFormat(YAML::Flow);
        
        out << YAML::BeginMap;
        out << YAML::Key << "name" << YAML::Value << "John";
        out << YAML::Key << "items" << YAML::Value;
        out << YAML::BeginSeq;
        out << 1 << 2 << 3;
        out << YAML::EndSeq;
        out << YAML::EndMap;
        std::cout << out.c_str() << std::endl;
    }
    
    std::cout << "\n--- Manual Building with Block Format ---" << std::endl;
    {
        YAML::Emitter out;
        out.SetIndent(2);
        out.SetMapFormat(YAML::Block);
        out.SetSeqFormat(YAML::Block);
        
        out << YAML::BeginMap;
        out << YAML::Key << "name" << YAML::Value << "John";
        out << YAML::Key << "items" << YAML::Value;
        out << YAML::BeginSeq;
        out << 1 << 2 << 3;
        out << YAML::EndSeq;
        out << YAML::EndMap;
        std::cout << out.c_str() << std::endl;
    }
}

int main() {
    test_emitter_configuration();
    return 0;
}