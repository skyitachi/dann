#pragma once

#include "dann/types.h"

namespace dann {

template <typename T = float>
class VectorIndexV2 {
public:
    virtual ~VectorIndexV2() = default;

    virtual std::vector<InternalSearchResult> search(const std::vector<T>& query, int k = 10) = 0;
    virtual std::vector<InternalSearchResult> search_batch(const std::vector<T>& queries, int k = 10) = 0;
    virtual void build_index(std::vector<T>& vectors, const std::vector<int64_t>& ids) = 0;

    virtual std::string index_type() const = 0;
};
}