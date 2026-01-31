#pragma once

#include <memory>
#include <string>
#include <vector>

#include "dann/types.h"
#include "dann/vector_index.h"

namespace dann {

class Index {
public:
    Index(std::string name,
          int dimension,
          int shard_count = 1,
          const std::string& index_type = "IVF",
          int hnsw_m = 16,
          int hnsw_ef_construction = 100);

    const std::string& name() const;

    bool add_vectors(const std::vector<float>& vectors, const std::vector<int64_t>& ids);
    bool add_vectors_bulk(const std::vector<float>& vectors,
                          const std::vector<int64_t>& ids,
                          int batch_size = 1000);

    std::vector<InternalSearchResult> search(const std::vector<float>& query, int k = 10);

    bool remove_vector(int64_t id);
    bool update_vector(int64_t id, const std::vector<float>& new_vector);

    void reset();

    size_t size() const;
    int dimension() const;
    std::string index_type() const;

    int shard_count() const;
    std::shared_ptr<VectorIndex> shard(int shard_id) const;

private:
    std::string name_;
    int dimension_;
    std::vector<std::shared_ptr<VectorIndex>> shards_;

    int shard_id_for_document(int64_t id) const;
};

} // namespace dann
