#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <dann/index_meta_data.h>

using json = nlohmann::json;
using dann::IndexMetaData;
using dann::NodeInfo;

TEST(IndexMetaDataTest, SerializeToJson) {
    IndexMetaData metadata;
    metadata.index = "test_index_001";
    metadata.node_ids = {"node_id1", "node_id2", "node_id3"};
    metadata.type = "ivf";
    metadata.dimension = 128;
    
    metadata.routing_table["node_id1"] = NodeInfo("coordinate", "main_index_file_path");
    metadata.routing_table["node_id2"] = NodeInfo("data", "data_file_path");
    metadata.routing_table["node_id3"] = NodeInfo("data", "data_file_path");

    json j = json::object();
    j["index"] = metadata.index;
    j["node_ids"] = metadata.node_ids;
    j["type"] = metadata.type;
    j["dimension"] = metadata.dimension;
    
    json routing_table = json::object();
    for (const auto& [node_id, node_info] : metadata.routing_table) {
        routing_table[node_id] = {
            {"role", node_info.role},
            {"index_file", node_info.index_file}
        };
    }
    j["routing_table"] = routing_table;

    std::string expected_json = R"({
        "index": "test_index_001",
        "node_ids": ["node_id1", "node_id2", "node_id3"],
        "type": "ivf",
        "dimension": 128,
        "routing_table": {
            "node_id1": {
                "role": "coordinate",
                "index_file": "main_index_file_path"
            },
            "node_id2": {
                "role": "data",
                "index_file": "data_file_path"
            },
            "node_id3": {
                "role": "data",
                "index_file": "data_file_path"
            }
        }
    })";

    json expected = json::parse(expected_json);
    EXPECT_EQ(j, expected);
}

TEST(IndexMetaDataTest, DeserializeFromJson) {
    std::string json_str = R"({
        "index": "test_index_001",
        "node_ids": ["node_id1", "node_id2", "node_id3"],
        "type": "ivf",
        "dimension": 128,
        "routing_table": {
            "node_id1": {
                "role": "coordinate",
                "index_file": "main_index_file_path"
            },
            "node_id2": {
                "role": "data",
                "index_file": "data_file_path"
            },
            "node_id3": {
                "role": "data",
                "index_file": "data_file_path"
            }
        }
    })";

    json j = json::parse(json_str);

    IndexMetaData metadata;
    metadata.index = j["index"];
    metadata.node_ids = j["node_ids"].get<std::vector<std::string>>();
    metadata.type = j["type"];
    metadata.dimension = j["dimension"];
    
    for (auto& [node_id, node_info_json] : j["routing_table"].items()) {
        NodeInfo node_info;
        node_info.role = node_info_json["role"];
        node_info.index_file = node_info_json["index_file"];
        metadata.routing_table[node_id] = node_info;
    }

    EXPECT_EQ(metadata.index, "test_index_001");
    EXPECT_EQ(metadata.node_ids.size(), 3);
    EXPECT_EQ(metadata.node_ids[0], "node_id1");
    EXPECT_EQ(metadata.type, "ivf");
    EXPECT_EQ(metadata.dimension, 128);
    EXPECT_EQ(metadata.routing_table.size(), 3);
    
    auto& node1 = metadata.routing_table["node_id1"];
    EXPECT_EQ(node1.role, "coordinate");
    EXPECT_EQ(node1.index_file, "main_index_file_path");
    
    auto& node2 = metadata.routing_table["node_id2"];
    EXPECT_EQ(node2.role, "data");
    EXPECT_EQ(node2.index_file, "data_file_path");
}

TEST(IndexMetaDataTest, RoundTripSerialization) {
    IndexMetaData original;
    original.index = "round_trip_test";
    original.node_ids = {"node_a", "node_b"};
    original.type = "ivf";
    original.dimension = 64;
    
    original.routing_table["node_a"] = NodeInfo("coordinate", "/path/to/coord.idx");
    original.routing_table["node_b"] = NodeInfo("data", "/path/to/data.idx");

    json j = json::object();
    j["index"] = original.index;
    j["node_ids"] = original.node_ids;
    j["type"] = original.type;
    j["dimension"] = original.dimension;
    
    json routing_table = json::object();
    for (const auto& [node_id, node_info] : original.routing_table) {
        routing_table[node_id] = {
            {"role", node_info.role},
            {"index_file", node_info.index_file}
        };
    }
    j["routing_table"] = routing_table;

    IndexMetaData deserialized;
    deserialized.index = j["index"];
    deserialized.node_ids = j["node_ids"].get<std::vector<std::string>>();
    deserialized.type = j["type"];
    deserialized.dimension = j["dimension"];
    
    for (auto& [node_id, node_info_json] : j["routing_table"].items()) {
        NodeInfo node_info;
        node_info.role = node_info_json["role"];
        node_info.index_file = node_info_json["index_file"];
        deserialized.routing_table[node_id] = node_info;
    }

    EXPECT_EQ(original.index, deserialized.index);
    EXPECT_EQ(original.node_ids, deserialized.node_ids);
    EXPECT_EQ(original.type, deserialized.type);
    EXPECT_EQ(original.dimension, deserialized.dimension);
    EXPECT_EQ(original.routing_table.size(), deserialized.routing_table.size());
    
    for (const auto& [node_id, node_info] : original.routing_table) {
        EXPECT_TRUE(deserialized.routing_table.count(node_id));
        EXPECT_EQ(node_info.role, deserialized.routing_table[node_id].role);
        EXPECT_EQ(node_info.index_file, deserialized.routing_table[node_id].index_file);
    }
}

TEST(IndexMetaDataTest, EmptyRoutingTable) {
    IndexMetaData metadata;
    metadata.index = "empty_test";
    metadata.type = "ivf";
    metadata.dimension = 256;

    json j = json::object();
    j["index"] = metadata.index;
    j["node_ids"] = metadata.node_ids;
    j["type"] = metadata.type;
    j["dimension"] = metadata.dimension;
    j["routing_table"] = json::object();

    EXPECT_TRUE(j["routing_table"].empty());
}
