#include "dann/vector_index.h"
#include <faiss/IndexFlat.h>
#include <faiss/IndexHNSW.h>
#include <faiss/IndexIDMap.h>
#include <chrono>
#include <stdexcept>

namespace dann {

VectorIndex::VectorIndex(int dimension,
                         const std::string& index_type,
                         int hnsw_m,
                         int hnsw_ef_construction)
    : dimension_(dimension),
      index_type_(index_type),
      hnsw_m_(hnsw_m),
      hnsw_ef_construction_(hnsw_ef_construction),
      version_(0) {
    if (dimension <= 0) {
        throw std::invalid_argument("Dimension must be greater than 0");
    }
    create_index();
}

VectorIndex::~VectorIndex() = default;

bool VectorIndex::add_vectors(const std::vector<float>& vectors, const std::vector<int64_t>& ids) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!validate_vectors(vectors) || vectors.size() / dimension_ != ids.size()) {
        return false;
    }

    auto* id_index = dynamic_cast<faiss::IndexIDMap2*>(index_.get());
    if (!id_index) {
        return false;
    }

    const size_t num_vectors = ids.size();
    id_index->add_with_ids(static_cast<faiss::idx_t>(num_vectors), vectors.data(), ids.data());

    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    for (size_t i = 0; i < num_vectors; ++i) {
        pending_operations_.push_back(IndexOperation(IndexOperation::ADD, ids[i], now_ms, version_.load()));
    }
    ++version_;
    return true;
}

bool VectorIndex::add_vectors_bulk(const std::vector<float>& vectors,
                                   const std::vector<int64_t>& ids,
                                   int batch_size) {
    if (batch_size <= 0) {
        return false;
    }
    if (!validate_vectors(vectors) || vectors.size() / dimension_ != ids.size()) {
        return false;
    }

    const size_t total = ids.size();
    size_t offset = 0;
    while (offset < total) {
        const size_t batch_count = std::min(static_cast<size_t>(batch_size), total - offset);
        std::vector<float> batch_vectors;
        std::vector<int64_t> batch_ids;
        batch_vectors.reserve(batch_count * dimension_);
        batch_ids.reserve(batch_count);

        const size_t vector_offset = offset * static_cast<size_t>(dimension_);
        batch_vectors.insert(batch_vectors.end(),
                             vectors.begin() + static_cast<long>(vector_offset),
                             vectors.begin() + static_cast<long>(vector_offset + batch_count * dimension_));
        batch_ids.insert(batch_ids.end(), ids.begin() + static_cast<long>(offset),
                         ids.begin() + static_cast<long>(offset + batch_count));

        if (!add_vectors(batch_vectors, batch_ids)) {
            return false;
        }
        offset += batch_count;
    }

    return true;
}

std::vector<SearchResult> VectorIndex::search(const std::vector<float>& query, int k) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SearchResult> results;
    if (!validate_vectors(query) || k <= 0 || size() == 0) {
        return results;
    }

    std::vector<faiss::idx_t> labels(static_cast<size_t>(k));
    std::vector<float> distances(static_cast<size_t>(k));
    index_->search(1, query.data(), k, distances.data(), labels.data());

    for (int i = 0; i < k; ++i) {
        if (labels[static_cast<size_t>(i)] < 0) {
            continue;
        }
        results.push_back(create_search_result(labels[static_cast<size_t>(i)], distances[static_cast<size_t>(i)]));
    }
    return results;
}

std::vector<SearchResult> VectorIndex::search_batch(const std::vector<float>& queries, int k) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SearchResult> results;
    if (!validate_vectors(queries) || k <= 0 || size() == 0) {
        return results;
    }

    const size_t num_queries = queries.size() / static_cast<size_t>(dimension_);
    std::vector<faiss::idx_t> labels(num_queries * static_cast<size_t>(k));
    std::vector<float> distances(num_queries * static_cast<size_t>(k));
    index_->search(static_cast<faiss::idx_t>(num_queries), queries.data(), k, distances.data(), labels.data());

    results.reserve(num_queries * static_cast<size_t>(k));
    for (size_t qi = 0; qi < num_queries; ++qi) {
        for (int ki = 0; ki < k; ++ki) {
            const size_t idx = qi * static_cast<size_t>(k) + static_cast<size_t>(ki);
            if (labels[idx] < 0) {
                continue;
            }
            results.push_back(create_search_result(labels[idx], distances[idx]));
        }
    }

    return results;
}

bool VectorIndex::remove_vector(int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    faiss::IDSelectorArray selector(1, &id);
    const faiss::idx_t removed = index_->remove_ids(selector);
    if (removed > 0) {
        const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();
        pending_operations_.push_back(IndexOperation(IndexOperation::DELETE, id, now_ms, version_.load()));
        ++version_;
        return true;
    }
    return false;
}

bool VectorIndex::update_vector(int64_t id, const std::vector<float>& new_vector) {
    if (new_vector.size() != static_cast<size_t>(dimension_)) {
        return false;
    }
    if (!remove_vector(id)) {
        return false;
    }
    return add_vectors(new_vector, std::vector<int64_t>{id});
}

bool VectorIndex::save_index(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        faiss::write_index(index_.get(), file_path.c_str());
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool VectorIndex::load_index(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        index_.reset(faiss::read_index(file_path.c_str()));
        ++version_;
        pending_operations_.clear();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void VectorIndex::reset_index() {
    std::lock_guard<std::mutex> lock(mutex_);
    create_index();
    pending_operations_.clear();
    ++version_;
}

size_t VectorIndex::size() const {
    return index_ ? static_cast<size_t>(index_->ntotal) : 0;
}

int VectorIndex::dimension() const {
    return dimension_;
}

std::string VectorIndex::index_type() const {
    return index_type_;
}

uint64_t VectorIndex::get_version() const {
    return version_.load();
}

void VectorIndex::set_version(uint64_t version) {
    version_.store(version);
}

std::vector<IndexOperation> VectorIndex::get_pending_operations() {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_operations_;
}

void VectorIndex::clear_pending_operations() {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_operations_.clear();
}

void VectorIndex::create_index() {
    faiss::Index* base_index = nullptr;
    if (index_type_ == "HNSW" || index_type_ == "hnsw") {
        auto* hnsw_index = new faiss::IndexHNSWFlat(dimension_, hnsw_m_);
        hnsw_index->hnsw.efConstruction = hnsw_ef_construction_;
        base_index = hnsw_index;
    } else {
        base_index = new faiss::IndexFlatL2(dimension_);
    }
    index_.reset(new faiss::IndexIDMap2(base_index));
}

bool VectorIndex::validate_vectors(const std::vector<float>& vectors) {
    return !vectors.empty() && (vectors.size() % static_cast<size_t>(dimension_) == 0);
}

SearchResult VectorIndex::create_search_result(int64_t id, float distance) const {
    return SearchResult(id, distance, {});
}

} // namespace dann
