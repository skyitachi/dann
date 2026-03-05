//
// Created by skyitachi on 2026/3/1.
//
#include "dann/ivf_shard.h"

namespace dann {
IndexIVFShard::IndexIVFShard(int shard_id, std::string node_id): shard_id_(shard_id), node_id_(std::move(node_id)) {}

void IndexIVFShard::add_posting(int centroid, const InvertedList &posting) {
  auto it = postings_.find(centroid);
  if (it == postings_.end()) {
    postings_[centroid] = InvertedList();
    it = postings_.find(centroid);
  }
  it->second.vector_ids.insert(it->second.vector_ids.end(), posting.vector_ids.begin(), posting.vector_ids.end());
  it->second.vectors.insert(it->second.vectors.end(), posting.vector_ids.begin(), posting.vector_ids.end());
}

std::vector<InternalSearchResult> IndexIVFShard::search(const std::vector<int64_t>& centroid_ids, const std::vector<float>& queries, int k) {
  for (const auto& centroid_id : centroid_ids) {
    auto it = postings_.find(centroid_id);
    if (it == postings_.end()) {
      continue;
    }
  }

}

} // namespace dann
