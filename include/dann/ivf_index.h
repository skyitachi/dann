#pragma once

#include "dann/vector_index_v2.h"

namespace dann {

template <typename T = float>
class IVFIndex : public VectorIndexV2<T> {
public:
    IVFIndex(int dimension, int nlist = 0, int nprobe = 0, const std::string index_build_path = "");
    ~IVFIndex() override = default;

    std::vector<InternalSearchResult> search(const std::vector<T>& query, int k = 10) override;
    std::vector<InternalSearchResult> search_batch(const std::vector<T>& queries, int k = 10) override;
    void build_index(std::vector<T>& vectors, const std::vector<int64_t>& ids) override;

    std::string index_type() const override {
        return "IVF";
    }

private:
    int dimension_;
    int nlist_;
    int nprobe_;
    std::string index_build_path_;
};

}