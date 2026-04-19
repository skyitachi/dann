//
// Created by skyitachi on 2026/3/1.
//
#include "dann/ivf_shard.h"

#include "dann/logger.h"
#include "dann/utils.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <unistd.h>

namespace dann {
IndexIVFShard::IndexIVFShard(int d, int shard_id, std::string node_id):
  dimension_(d), shard_id_(shard_id), node_id_(std::move(node_id)) {}

void IndexIVFShard::add_posting(int64_t centroid, const InvertedList &posting) {
  auto it = postings_.find(centroid);
  if (it == postings_.end()) {
    postings_[centroid] = InvertedList();
    it = postings_.find(centroid);
  }
  it->second.vector_ids.insert(it->second.vector_ids.end(), posting.vector_ids.begin(), posting.vector_ids.end());
  it->second.vectors.insert(it->second.vectors.end(), posting.vectors.begin(), posting.vectors.end());
}

std::vector<InternalSearchResult> IndexIVFShard::search(const std::vector<int64_t>& centroid_ids, const std::vector<float>& query, int k) {
  std::vector<InternalSearchResult> result;
  result.reserve(k);
  std::vector<float> temp_vectors;
  std::vector<int64_t> temp_ids;
  InternalSearchResultQueue result_queue;
  for (const auto& centroid_id : centroid_ids) {
    auto it = postings_.find(centroid_id);
    if (it == postings_.end()) {
      continue;
    }
    auto result_with_distance = find_closest_k_with_distance(it->second.vectors,query,dimension_, it->second.vector_ids.size(), k);
    for (const auto &item: result_with_distance) {
      // LOG_INFOF("index=%d, distance=%f", item.index, item.distance);
      if (result_queue.size() < k || item.distance < result_queue.top().distance) {
        std::vector<float> vector(it->second.vectors.begin() + item.index * dimension_, 
                                  it->second.vectors.begin() + (item.index + 1) * dimension_);
        if (result_queue.size() == k) {
          result_queue.pop();
        }
        result_queue.emplace(it->second.vector_ids[item.index], item.distance, vector);
      }
    }
  }
  while (!result_queue.empty())
  {
    result.push_back(result_queue.top());
    result_queue.pop();
  }
  std::reverse(result.begin(), result.end());
  return result;
}

void IndexIVFShard::add_postings( const std::unordered_map<int64_t, InvertedList> &postings) {
  for (const auto& [c, inv]: postings) {
    add_posting(c, inv);
  }
}

size_t IndexIVFShard::total_vectors() const {
  size_t total = 0;
  for (const auto& [_, inv] : postings_) {
    total += inv.vector_ids.size();
  }
  return total;
}

uint32_t IndexIVFShard::CalculateChecksum(const uint8_t* data, size_t size) {
  uint32_t hash = 0;
  for (size_t i = 0; i < size; ++i) {
    hash += static_cast<uint32_t>(data[i]);
    hash ^= (hash << 13);
    hash ^= (hash >> 17);
    hash ^= (hash << 5);
  }
  return hash;
}

Status IndexIVFShard::AtomicWrite(const std::string& path, const void* data, size_t size) {
  std::string tmp_path = path + ".tmp";
  
  FILE* fp = fopen(tmp_path.c_str(), "wb");
  if (!fp) {
    return Status::IOError("Failed to open file for writing: " + tmp_path);
  }
  
  size_t written = fwrite(data, 1, size, fp);
  if (written != size) {
    fclose(fp);
    std::filesystem::remove(tmp_path);
    return Status::IOError("Failed to write file: " + tmp_path);
  }
  
  fflush(fp);
  fsync(fileno(fp));
  fclose(fp);
  
  std::error_code ec;
  std::filesystem::rename(tmp_path, path, ec);
  if (ec) {
    std::filesystem::remove(tmp_path);
    return Status::IOError("Failed to rename file: " + tmp_path + " -> " + path);
  }
  
  return Status::OK();
}

Status IndexIVFShard::Save(const std::string& file_path) const {
  if (dimension_ <= 0) {
    return Status::InvalidArgument("Invalid dimension");
  }
  
  std::filesystem::path p(file_path);
  std::filesystem::path dir = p.parent_path();
  if (!dir.empty() && !std::filesystem::exists(dir)) {
    std::error_code ec;
    if (!std::filesystem::create_directories(dir, ec)) {
      return Status::IOError("Failed to create directory: " + dir.string());
    }
  }
  
  std::vector<uint8_t> buffer;
  
  struct Header {
    uint32_t magic;
    uint32_t version;
    uint32_t shard_id;
    uint32_t dimension;
    uint32_t num_centroids;
    uint64_t num_vectors;
    uint32_t checksum;
    uint32_t reserved;
  };
  
  Header header;
  header.magic = POSTING_FILE_MAGIC;
  header.version = POSTING_FILE_VERSION;
  header.shard_id = static_cast<uint32_t>(shard_id_);
  header.dimension = static_cast<uint32_t>(dimension_);
  header.num_centroids = static_cast<uint32_t>(postings_.size());
  header.num_vectors = static_cast<uint64_t>(total_vectors());
  header.checksum = 0;
  header.reserved = 0;
  
  size_t data_size = sizeof(Header);
  for (const auto& [centroid_id, inv] : postings_) {
    data_size += sizeof(int64_t);
    data_size += sizeof(uint32_t);
    data_size += inv.vector_ids.size() * sizeof(int64_t);
    data_size += inv.vectors.size() * sizeof(float);
  }
  data_size += sizeof(uint32_t);
  
  buffer.resize(data_size);
  uint8_t* ptr = buffer.data();
  
  std::memcpy(ptr, &header, sizeof(Header));
  ptr += sizeof(Header);
  
  for (const auto& [centroid_id, inv] : postings_) {
    int64_t cid = centroid_id;
    uint32_t num_vec = static_cast<uint32_t>(inv.vector_ids.size());
    
    std::memcpy(ptr, &cid, sizeof(int64_t));
    ptr += sizeof(int64_t);
    
    std::memcpy(ptr, &num_vec, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    
    if (num_vec > 0) {
      std::memcpy(ptr, inv.vector_ids.data(), num_vec * sizeof(int64_t));
      ptr += num_vec * sizeof(int64_t);
      
      std::memcpy(ptr, inv.vectors.data(), inv.vectors.size() * sizeof(float));
      ptr += inv.vectors.size() * sizeof(float);
    }
  }
  
  uint32_t footer_magic = POSTING_FILE_FOOTER_MAGIC;
  std::memcpy(ptr, &footer_magic, sizeof(uint32_t));
  
  header.checksum = CalculateChecksum(buffer.data() + sizeof(Header), 
                                       data_size - sizeof(Header) - sizeof(uint32_t));
  std::memcpy(buffer.data(), &header, sizeof(Header));
  
  return AtomicWrite(file_path, buffer.data(), buffer.size());
}

Status IndexIVFShard::Load(const std::string& file_path) {
  if (!std::filesystem::exists(file_path)) {
    return Status::NotExist("File does not exist: " + file_path);
  }
  
  std::ifstream ifs(file_path, std::ios::binary);
  if (!ifs.is_open()) {
    return Status::IOError("Failed to open file for reading: " + file_path);
  }
  
  struct Header {
    uint32_t magic;
    uint32_t version;
    uint32_t shard_id;
    uint32_t dimension;
    uint32_t num_centroids;
    uint64_t num_vectors;
    uint32_t checksum;
    uint32_t reserved;
  };
  
  Header header;
  ifs.read(reinterpret_cast<char*>(&header), sizeof(Header));
  if (!ifs.good()) {
    return Status::Corruption("Failed to read header from file: " + file_path);
  }
  
  if (header.magic != POSTING_FILE_MAGIC) {
    return Status::Corruption("Invalid magic number in file: " + file_path);
  }
  
  if (header.version != POSTING_FILE_VERSION) {
    return Status::Corruption("Unsupported version in file: " + file_path);
  }
  
  if (header.dimension != static_cast<uint32_t>(dimension_)) {
    return Status::Corruption("Dimension mismatch in file: " + file_path + 
                              ", expected " + std::to_string(dimension_) + 
                              ", got " + std::to_string(header.dimension));
  }
  
  postings_.clear();
  
  for (uint32_t i = 0; i < header.num_centroids; ++i) {
    int64_t centroid_id;
    uint32_t num_vec;
    
    ifs.read(reinterpret_cast<char*>(&centroid_id), sizeof(int64_t));
    if (!ifs.good()) {
      return Status::Corruption("Failed to read centroid id from file: " + file_path);
    }
    
    ifs.read(reinterpret_cast<char*>(&num_vec), sizeof(uint32_t));
    if (!ifs.good()) {
      return Status::Corruption("Failed to read vector count from file: " + file_path);
    }
    
    InvertedList inv;
    if (num_vec > 0) {
      inv.vector_ids.resize(num_vec);
      ifs.read(reinterpret_cast<char*>(inv.vector_ids.data()), 
               static_cast<std::streamsize>(num_vec * sizeof(int64_t)));
      if (!ifs.good()) {
        return Status::Corruption("Failed to read vector ids from file: " + file_path);
      }
      
      inv.vectors.resize(static_cast<size_t>(num_vec) * header.dimension);
      ifs.read(reinterpret_cast<char*>(inv.vectors.data()),
               static_cast<std::streamsize>(inv.vectors.size() * sizeof(float)));
      if (!ifs.good()) {
        return Status::Corruption("Failed to read vectors from file: " + file_path);
      }
    }
    
    postings_[centroid_id] = std::move(inv);
  }
  
  uint32_t footer_magic;
  ifs.read(reinterpret_cast<char*>(&footer_magic), sizeof(uint32_t));
  if (!ifs.good()) {
    return Status::Corruption("Failed to read footer from file: " + file_path);
  }
  
  if (footer_magic != POSTING_FILE_FOOTER_MAGIC) {
    return Status::Corruption("Invalid footer magic in file: " + file_path);
  }
  
  ifs.close();
  return Status::OK();
}

} // namespace dann
