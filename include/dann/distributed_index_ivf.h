//
// Created by skyitachi on 2026/2/27.
//

#ifndef DANN_DISTRIBUTED_INDEX_IVF_H
#define DANN_DISTRIBUTED_INDEX_IVF_H

#include <memory>
#include <set>
#include <unordered_map>

#include "dann/clustering.h"
#include "dann/ivf_shard.h"
#include "dann/types.h"
#include "dann/index_shard.h"

namespace dann
{

class DistributedIndexIVF: public IndexShard {
public:
    DistributedIndexIVF(std::string name, int d, int shards, std::vector<std::string> nodes);
    bool add_vectors(const std::vector<float>& vectors, const std::vector<int64_t>& ids) override;
    void build_index(const std::vector<float>& vectors, const std::vector<int64_t>& ids);
    std::vector<InternalSearchResult> search(const std::vector<float>& query, int k) override;
    std::string index_type() const override;
    size_t size() override { return 0; }
    int dimension() const override;
    bool load_index(const std::string &index_path) override;
    ~DistributedIndexIVF() = default;

private:
    std::vector<float> sample_training_vectors(const std::vector<float>& vectors, int64_t n_train) const;
    int64_t find_closest_optimized(const float* x, const float* y, int d, int n) const;

    std::string name_;
    int dimension_;
    bool is_trained_;
    // number of vectors initially
    int64_t ntotal_;
    int shard_counts_;
    int nprobe_;

    std::unique_ptr<Clustering> clustering_;
    std::vector<float> global_centroids_;
    std::vector<int> global_centroid_ids_;

    std::unordered_map<int, std::unique_ptr<IndexIVFShard>> shards_;
    // cluster nodes
    std::vector<std::string> nodes_;
    std::set<int> shard_ids_;

};

}


#endif //DANN_DISTRIBUTED_INDEX_IVF_H