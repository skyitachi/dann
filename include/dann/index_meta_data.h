#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace dann {

struct NodeInfo {
    std::string role;
    std::string index_file;

    NodeInfo() = default;
    NodeInfo(const std::string& r, const std::string& f)
        : role(r), index_file(f) {}

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(NodeInfo, role, index_file)
};

class IndexMetaData {
public:
    IndexMetaData() = default;
    ~IndexMetaData() = default;

    std::string index;
    std::vector<std::string> node_ids;
    std::string type;
    int dimension;
    std::unordered_map<std::string, NodeInfo> routing_table;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(IndexMetaData, index, node_ids, type, dimension, routing_table)
};

}
