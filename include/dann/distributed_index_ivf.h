//
// Created by skyitachi on 2026/2/27.
//

#ifndef DANN_DISTRIBUTED_INDEX_IVF_H
#define DANN_DISTRIBUTED_INDEX_IVF_H

#include <atomic>
#include <filesystem>
#include <memory>
#include <set>
#include <shared_mutex>
#include <unordered_map>

#include "dann/clustering.h"
#include "dann/ivf_shard.h"
#include "dann/types.h"
#include "dann/index_shard.h"
#include "dann/metadata_storage.h"
#include "dann/main_index_storage.h"
#include "dann/status.h"

namespace dann
{

struct IndexPersistenceMetadata {
    std::string index_name;
    int dimension;
    int shard_count;
    int nlist;
    int nprobe;
    std::string index_type;
    
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(IndexPersistenceMetadata, 
                                    index_name, dimension, shard_count, 
                                    nlist, nprobe, index_type)
};

class DistributedIndexIVF: public IndexShard {
public:
    DistributedIndexIVF(std::string name, std::string node_id, std::shared_ptr<MetaDataStorage> meta_data_storage);
    DistributedIndexIVF(std::string name, int d, int shards, std::vector<std::string> nodes);
    DistributedIndexIVF(std::string name, int d, int shards, int nlist, int nprobe, std::vector<std::string> nodes);
    
    DistributedIndexIVF(std::string name, int d, int total_shards, int shard_id, std::string node_id);
    
    bool add_vectors(const std::vector<float>& vectors, const std::vector<int64_t>& ids) override;
    void build_index(const std::vector<float>& vectors, const std::vector<int64_t>& ids);
    std::vector<InternalSearchResult> search(const std::vector<float>& query, int k) override;
    std::string index_type() const override;
    size_t size() override;
    int dimension() const override;
    bool load_index(const std::string &index_path) override;
    bool save_index(const std::string &index_path);
    bool is_dirty() const { return dirty_.load(); }
    void set_dirty(bool dirty) { dirty_.store(dirty); }
    
    bool load_shard_only(const std::string& base_path, int shard_id);
    int total_shards() const { return shard_counts_; }
    int current_shard_id() const { return current_shard_id_; }
    
    ~DistributedIndexIVF() = default;

private:
    std::vector<float> sample_training_vectors(const std::vector<float>& vectors, int64_t n_train) const;
    int64_t find_closest_optimized(const float* x, const float* y, int d, int n) const;
    
    Status SaveMetadata(const std::string& base_path) const;
    Status LoadMetadata(const std::string& base_path, IndexPersistenceMetadata* meta);
    Status SaveCentroids(const std::string& base_path) const;
    Status LoadCentroids(const std::string& base_path);
    Status SaveShards(const std::string& base_path) const;
    Status LoadShards(const std::string& base_path);

    std::string name_;
    int dimension_;
    bool is_trained_;
    int64_t ntotal_;
    int shard_counts_;
    int current_shard_id_{-1};
    bool is_shard_mode_{false};
    int nlist_{-1};
    int nprobe_;
    std::atomic<bool> dirty_{false};

    std::shared_ptr<MetaDataStorage> meta_data_storage_;

    std::unique_ptr<Clustering> clustering_;
    std::vector<float> global_centroids_;
    std::vector<int> global_centroid_ids_;
    std::unique_ptr<MainIndexStorage> main_index_storage_;

    std::unordered_map<int, std::unique_ptr<IndexIVFShard>> shards_;
    std::vector<std::string> nodes_;
    std::set<int> shard_ids_;
    std::shared_ptr<IndexMetaData> meta_data_;
    mutable std::shared_mutex rw_mutex_;

};

}


#endif //DANN_DISTRIBUTED_INDEX_IVF_H