//
// Created by skyitachi on 2026/2/27.
//

#ifndef DANN_INF_SHARD_H
#define DANN_INF_SHARD_H
#include "dann/types.h"
#include "unordered_map"

namespace dann
{

struct InvertedList
{
    std::vector<int64_t> vector_ids;
    std::vector<float> vectors;
};

class IndexIVFShard {
public:
    std::vector<InternalSearchResult> search(const std::vector<int64_t>& centroid_ids, const std::vector<float>& queries, int k);
private:
    int shard_id_;
    std::string node_id_;
    std::unordered_map<int64_t, InvertedList> postings_;

};
}
#endif //DANN_INF_SHARD_H