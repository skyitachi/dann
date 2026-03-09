//
// Created by skyitachi on 2026/3/1.
//
#include "dann/ivf_shard.h"

#include "dann/logger.h"
#include "dann/utils.h"
#include <algorithm>

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

} // namespace dann
