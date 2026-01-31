#include "dann/index.h"

#include <algorithm>
#include <stdexcept>

namespace dann {

Index::Index(std::string name,
             int dimension,
             int shard_count,
             const std::string& index_type,
             int hnsw_m,
             int hnsw_ef_construction)
    : name_(std::move(name)), dimension_(dimension) {
    if (dimension_ <= 0) {
        throw std::invalid_argument("Dimension must be greater than 0");
    }
    if (shard_count <= 0) {
        throw std::invalid_argument("Shard count must be greater than 0");
    }

    shards_.reserve(static_cast<size_t>(shard_count));
    for (int i = 0; i < shard_count; ++i) {
        shards_.push_back(std::make_shared<VectorIndex>(dimension_, index_type, hnsw_m, hnsw_ef_construction));
    }
}

const std::string& Index::name() const {
    return name_;
}

int Index::shard_id_for_document(int64_t id) const {
    if (shards_.empty()) {
        return 0;
    }
    const uint64_t h = std::hash<int64_t>{}(id);
    return static_cast<int>(h % static_cast<uint64_t>(shards_.size()));
}

bool Index::add_vectors(const std::vector<float>& vectors, const std::vector<int64_t>& ids) {
    if (ids.empty()) {
        return false;
    }
    if (vectors.empty() || vectors.size() % static_cast<size_t>(dimension_) != 0) {
        return false;
    }
    if (vectors.size() / static_cast<size_t>(dimension_) != ids.size()) {
        return false;
    }

    if (shards_.size() == 1) {
        return shards_[0]->add_vectors(vectors, ids);
    }

    std::vector<std::vector<float>> shard_vectors(shards_.size());
    std::vector<std::vector<int64_t>> shard_ids(shards_.size());

    for (size_t i = 0; i < ids.size(); ++i) {
        const int shard_id = shard_id_for_document(ids[i]);
        shard_ids[static_cast<size_t>(shard_id)].push_back(ids[i]);

        auto& vecs = shard_vectors[static_cast<size_t>(shard_id)];
        const size_t base = i * static_cast<size_t>(dimension_);
        vecs.insert(vecs.end(),
                    vectors.begin() + static_cast<long>(base),
                    vectors.begin() + static_cast<long>(base + static_cast<size_t>(dimension_)));
    }

    bool ok = true;
    for (size_t shard = 0; shard < shards_.size(); ++shard) {
        if (shard_ids[shard].empty()) {
            continue;
        }
        ok = ok && shards_[shard]->add_vectors(shard_vectors[shard], shard_ids[shard]);
    }

    return ok;
}

bool Index::add_vectors_bulk(const std::vector<float>& vectors,
                             const std::vector<int64_t>& ids,
                             int batch_size) {
    if (batch_size <= 0) {
        return false;
    }

    if (shards_.size() == 1) {
        return shards_[0]->add_vectors_bulk(vectors, ids, batch_size);
    }

    return add_vectors(vectors, ids);
}

std::vector<InternalSearchResult> Index::search(const std::vector<float>& query, int k) {
    std::vector<InternalSearchResult> merged;
    if (k <= 0) {
        return merged;
    }

    for (const auto& shard : shards_) {
        auto shard_results = shard->search(query, k);
        merged.insert(merged.end(), shard_results.begin(), shard_results.end());
    }

    std::sort(merged.begin(), merged.end(), [](const InternalSearchResult& a, const InternalSearchResult& b) {
        return a.distance < b.distance;
    });

    if (merged.size() > static_cast<size_t>(k)) {
        merged.resize(static_cast<size_t>(k));
    }

    return merged;
}

bool Index::remove_vector(int64_t id) {
    if (shards_.empty()) {
        return false;
    }

    const int shard_id = shard_id_for_document(id);
    return shards_[static_cast<size_t>(shard_id)]->remove_vector(id);
}

bool Index::update_vector(int64_t id, const std::vector<float>& new_vector) {
    if (shards_.empty()) {
        return false;
    }

    const int shard_id = shard_id_for_document(id);
    return shards_[static_cast<size_t>(shard_id)]->update_vector(id, new_vector);
}

void Index::reset() {
    for (const auto& shard : shards_) {
        shard->reset_index();
    }
}

size_t Index::size() const {
    size_t total = 0;
    for (const auto& shard : shards_) {
        total += shard->size();
    }
    return total;
}

int Index::dimension() const {
    return dimension_;
}

std::string Index::index_type() const {
    if (shards_.empty()) {
        return {};
    }
    return shards_[0]->index_type();
}

int Index::shard_count() const {
    return static_cast<int>(shards_.size());
}

std::shared_ptr<VectorIndex> Index::shard(int shard_id) const {
    if (shard_id < 0 || shard_id >= static_cast<int>(shards_.size())) {
        return nullptr;
    }
    return shards_[static_cast<size_t>(shard_id)];
}

} // namespace dann
