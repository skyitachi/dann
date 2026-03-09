//
// Created by skyitachi on 2026/3/9.
//

#ifndef DANN_INDEX_SHARD_H
#define DANN_INDEX_SHARD_H
#include "dann/types.h"

namespace dann {

class IndexShard {
public:
    // Core operations
    virtual bool add_vectors(const std::vector<float>& vectors, const std::vector<int64_t>& ids) = 0;
    virtual std::vector<InternalSearchResult> search(const std::vector<float>& query, int k = 10) = 0;
    virtual size_t size() = 0;
    virtual std::string index_type() = 0;

    virtual ~IndexShard() = 0;
};

}
#endif // DANN_INDEX_SHARD_H
