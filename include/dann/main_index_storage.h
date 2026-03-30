#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include "dann/status.h"

namespace dann {

constexpr uint32_t MAIN_INDEX_MAGIC = 0x44414E4E;
constexpr uint32_t MAIN_INDEX_VERSION = 1;
constexpr uint32_t MAIN_INDEX_FOOTER_MAGIC = 0x4E4E4144;

struct PostingAssignment {
    uint32_t centroid_start;
    uint32_t centroid_end;
    uint32_t node_id_index;
};

class MainIndexStorage {
public:
    MainIndexStorage();
    explicit MainIndexStorage(uint32_t dimension);
    ~MainIndexStorage() = default;

    void SetDimension(uint32_t dim) { dimension_ = dim; }
    uint32_t dimension() const { return dimension_; }
    uint32_t total_centroids() const { return total_centroids_; }
    uint32_t num_assignments() const { return static_cast<uint32_t>(assigns_.size()); }
    uint32_t num_unique_nodes() const { return static_cast<uint32_t>(unique_node_ids_.size()); }

    void AddCentroid(const float* data);
    void AddAssignment(uint32_t centroid_start, uint32_t centroid_end, const std::string& node_id);
    
    const std::vector<float>& centroids() const { return centroids_; }
    const std::vector<PostingAssignment>& assignments() const { return assigns_; }
    const std::vector<std::string>& unique_node_ids() const { return unique_node_ids_; }

    const std::string& GetNodeId(uint32_t index) const {
        return unique_node_ids_[index];
    }

    Status Save(const std::string& file_path) const;
    Status Load(const std::string& file_path);

    void Clear();

private:
    uint32_t magic_number_{MAIN_INDEX_MAGIC};
    uint32_t version_{MAIN_INDEX_VERSION};
    uint32_t total_centroids_{0};
    uint32_t dimension_{0};
    uint32_t checksum_{0};

    std::vector<std::string> unique_node_ids_;
    std::vector<PostingAssignment> assigns_;
    std::vector<float> centroids_;

    std::unordered_map<std::string, uint32_t> node_id_to_index_;

    uint32_t GetOrCreateNodeIdIndex(const std::string& node_id);
    static uint32_t CalculateChecksum(const uint8_t* data, size_t size);
};

}
