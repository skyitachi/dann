//
// Created by skyitachi on 2026/2/27.
//

#ifndef DANN_INF_SHARD_H
#define DANN_INF_SHARD_H

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include "dann/status.h"
#include "dann/types.h"

namespace dann
{

constexpr uint32_t POSTING_FILE_MAGIC = 0x504F5354;
constexpr uint32_t POSTING_FILE_FOOTER_MAGIC = 0x54534F50;
constexpr uint32_t POSTING_FILE_VERSION = 1;

struct InvertedList
{
    std::vector<int64_t> vector_ids;
    std::vector<float> vectors;
};

class IndexIVFShard {
public:
    IndexIVFShard(int d, int shard_id, std::string node_id);
    std::vector<InternalSearchResult> search(const std::vector<int64_t>& centroid_ids, const std::vector<float>& queries, int k);
    void add_postings(const std::unordered_map<int64_t, InvertedList>& postings);
    void add_posting(int64_t centroid, const InvertedList& posting);
    
    Status Save(const std::string& file_path) const;
    Status Load(const std::string& file_path);
    
    int dimension() const { return dimension_; }
    int shard_id() const { return shard_id_; }
    size_t num_postings() const { return postings_.size(); }
    size_t total_vectors() const;

private:
    int shard_id_;
    std::string node_id_;
    int dimension_;
    std::unordered_map<int64_t, InvertedList> postings_;
    
    static uint32_t CalculateChecksum(const uint8_t* data, size_t size);
    static Status AtomicWrite(const std::string& path, const void* data, size_t size);
};
}
#endif //DANN_INF_SHARD_H