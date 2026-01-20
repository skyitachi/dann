#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <faiss/Index.h>
#include <faiss/index_io.h>
#include "dann/types.h"

namespace dann {

class VectorIndex {
public:
    VectorIndex(int dimension,
                const std::string& index_type = "IVF",
                int hnsw_m = 16,
                int hnsw_ef_construction = 100);
    ~VectorIndex();
    
    // Core operations
    bool add_vectors(const std::vector<float>& vectors, const std::vector<int64_t>& ids);
    bool add_vectors_bulk(const std::vector<float>& vectors, const std::vector<int64_t>& ids, int batch_size = 1000);
    
    std::vector<SearchResult> search(const std::vector<float>& query, int k = 10);
    std::vector<SearchResult> search_batch(const std::vector<float>& queries, int k = 10);
    
    bool remove_vector(int64_t id);
    bool update_vector(int64_t id, const std::vector<float>& new_vector);
    
    // Index management
    bool save_index(const std::string& file_path);
    bool load_index(const std::string& file_path);
    void reset_index();
    
    // Metadata
    size_t size() const;
    int dimension() const;
    std::string index_type() const;
    
    // Consistency support
    uint64_t get_version() const;
    void set_version(uint64_t version);
    std::vector<IndexOperation> get_pending_operations();
    void clear_pending_operations();
    
private:
    std::unique_ptr<faiss::Index> index_;
    int dimension_;
    std::string index_type_;
    int hnsw_m_;
    int hnsw_ef_construction_;
    mutable std::mutex mutex_;
    std::atomic<uint64_t> version_;
    std::vector<IndexOperation> pending_operations_;
    
    void create_index();
    bool validate_vectors(const std::vector<float>& vectors);
    SearchResult create_search_result(int64_t id, float distance) const;
};

} // namespace dann
