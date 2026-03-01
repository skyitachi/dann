//
// Created by skyitachi on 2026/2/27.
//

#ifndef DANN_DISTRIBUTED_INDEX_IVF_H
#define DANN_DISTRIBUTED_INDEX_IVF_H

#include "dann/types.h"
#include "dann/ivf_shard.h"
#include "dann/clustering.h"
#include <unordered_map>
#include <memory>

namespace dann
{

class DistributedIndexIVF {
public:
    DistributedIndexIVF(std::string name, int d, int64_t ntotal);
    void build_index(const std::vector<float>& vectors, const std::vector<int64_t>& ids);
    std::vector<InternalSearchResult> search(const std::vector<float>& query, int k);
private:
    std::vector<float> sample_training_vectors(const std::vector<float>& vectors, int64_t n_train) const;
    
    std::string name_;
    int dimension_;
    bool is_trained_;
    // number of vectors initially
    int64_t ntotal_;

    std::unique_ptr<Clustering> clustering_;
    std::vector<float> global_centroids_;
    std::vector<int> global_centroid_ids_;

    std::unordered_map<int, std::unique_ptr<IndexIVFShard>> shards_;
    // cluster nodes
    std::vector<std::string> nodes_;

};

}


#endif //DANN_DISTRIBUTED_INDEX_IVF_H