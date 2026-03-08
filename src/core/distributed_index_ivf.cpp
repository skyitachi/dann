//
// Created by skyitachi on 2026/2/27.
//

#include "dann/distributed_index_ivf.h"

#include "dann/utils.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>
#include <random>

namespace dann
{
int64_t get_nlist(int64_t N) {
    int64_t nlist = N;
    if (N < 1000000) {
        nlist = 8 * sqrt(N);
    } else if (N < 10000000) {
        nlist = 65536;  // 2^16
    } else if (N < 100000000) {
        nlist = 262144; // 2^18
    } else if (N < 1000000000) {
        nlist = 1048576; // 2^20
    }
    return nlist;
}

DistributedIndexIVF::DistributedIndexIVF(std::string name, int d, int64_t n, int shards, std::vector<std::string> nodes):
    name_(std::move(name)), dimension_(d), ntotal_(n), shard_counts_(shards), nodes_(std::move(nodes)), is_trained_(false) {
    assert(shard_counts_ >= nodes.size() && shard_counts_ > 0);
    clustering_ = std::make_unique<Clustering>(d, get_nlist(n));
    // 将shards均分到nodes上
    int node_size = nodes_.size();
    for (int i = 0; i < shard_counts_; i++) {
        shards_[i] = std::make_unique<IndexIVFShard>(d, i, nodes_[i % node_size]);
    }
}

void DistributedIndexIVF::build_index_optimized_by_codex5_3(
    const std::vector<float>& vectors,
    const std::vector<int64_t>& ids) {
    assert(dimension_ != 0);
    assert(vectors.size() / dimension_ == ids.size());

    const int64_t num_vectors = static_cast<int64_t>(ids.size());
    if (num_vectors == 0) {
        global_centroids_.clear();
        global_centroid_ids_.clear();
        is_trained_ = false;
        return;
    }

    // 1) Sampling + clustering training
    const int64_t n_train = std::min(static_cast<int64_t>(clustering_->k) * 64, num_vectors);
    std::vector<float> train_vectors = sample_training_vectors(vectors, n_train);
    const int64_t actual_n_train = static_cast<int64_t>(train_vectors.size() / dimension_);

    clustering_->train(train_vectors, actual_n_train);
    global_centroids_ = clustering_->centroids;
    const int64_t num_centroids = static_cast<int64_t>(global_centroids_.size() / dimension_);

    global_centroid_ids_.resize(num_centroids);
    std::iota(global_centroid_ids_.begin(), global_centroid_ids_.end(), 0);

    // 2) First pass: count vectors per centroid to reserve exact capacity
    std::vector<int64_t> centroid_counts(num_centroids, 0);
    std::vector<int64_t> assignments(num_vectors, 0);
    for (int64_t i = 0; i < num_vectors; ++i) {
        const int64_t centroid = find_closest_optimized(
            global_centroids_.data(),
            vectors.data() + i * dimension_,
            dimension_,
            static_cast<int>(num_centroids));
        assignments[i] = centroid;
        ++centroid_counts[centroid];
    }

    // 3) Build postings using pre-sized vectors to reduce reallocation/copies
    std::vector<InvertedList> postings(num_centroids);
    for (int64_t centroid = 0; centroid < num_centroids; ++centroid) {
        const size_t c = static_cast<size_t>(centroid_counts[centroid]);
        postings[centroid].vectors.resize(c * static_cast<size_t>(dimension_));
        postings[centroid].vector_ids.resize(c);
    }

    std::vector<int64_t> cursor(num_centroids, 0);
    for (int64_t i = 0; i < num_vectors; ++i) {
        const int64_t centroid = assignments[i];
        const int64_t pos = cursor[centroid]++;

        float* dst = postings[centroid].vectors.data() + pos * dimension_;
        const float* src = vectors.data() + i * dimension_;
        std::copy(src, src + dimension_, dst);
        postings[centroid].vector_ids[static_cast<size_t>(pos)] = ids[static_cast<size_t>(i)];
    }

    // 4) Distribute postings to shards
    for (int64_t centroid = 0; centroid < num_centroids; ++centroid) {
        if (postings[centroid].vector_ids.empty()) {
            continue;
        }
        const int shard_id = static_cast<int>(centroid % shard_counts_);
        shards_[shard_id]->add_posting(static_cast<int>(centroid), std::move(postings[centroid]));
    }

    is_trained_ = true;
}

void DistributedIndexIVF::build_index(const std::vector<float>& vectors, const std::vector<int64_t>& ids) {
    assert(dimension_ != 0);
    assert(vectors.size() / dimension_ == ids.size());
    int64_t n_train = clustering_->k * 64;

    // 从vectors中随机选出n_train的d维向量
    std::vector<float> train_vectors = sample_training_vectors(vectors, n_train);
    int64_t actual_n_train = train_vectors.size() / dimension_;

    // 1. 训练好聚类中心
    clustering_->train(train_vectors, actual_n_train);
    global_centroids_ = clustering_->centroids;
    global_centroid_ids_.reserve(global_centroids_.size() / dimension_);
    std::iota(global_centroid_ids_.begin(), global_centroid_ids_.end(), 0);

    // 2. 构建global postings
    std::unordered_map<int64_t, InvertedList> postings;
    for (int i = 0; i < ids.size(); i++) {
        int64_t centroid = find_closest(&global_centroids_[0], &vectors[i * dimension_], dimension_, global_centroid_ids_.size());
        auto it = postings.find(global_centroid_ids_[centroid]);
        if (it == postings.end()) {
            postings[centroid] = InvertedList();
            it = postings.find(centroid);
        }
        it->second.vectors.insert(it->second.vectors.end(), vectors.begin() + i * dimension_, vectors.begin() + (i + 1) * dimension_);
        it->second.vector_ids.push_back(ids.at(i));
    }
    // 3. 将全局postings 均分到shards上
    for (const auto& [centroid, inv]: postings) {
        int shard_id = centroid % shard_counts_;
        shards_[shard_id]->add_posting(centroid, inv);
    }
    is_trained_ = true;
}

std::vector<InternalSearchResult> DistributedIndexIVF::search(const std::vector<float>& query, int k, int nprobe) {
    if (nprobe > global_centroid_ids_.size()) {
        nprobe = global_centroid_ids_.size();
    }
    // 从global_vectors中找到nprobe和query最近的向量
    std::vector<DistanceWithIndex> closest_centroids =
        find_closest_k_with_distance(&global_centroids_[0], &query[0], dimension_, global_centroid_ids_.size(), nprobe);

    std::vector<InternalSearchResult> results;
    std::unordered_map<int, std::vector<int64_t>> query_centroids_map;
    for (const auto& centroid : closest_centroids) {
        int shard_id = global_centroid_ids_[centroid.index] % shard_counts_;
        query_centroids_map[shard_id].push_back(global_centroid_ids_[centroid.index]);
    }
    for (const auto& [shard_id, centroids] : query_centroids_map) {
        auto shard_result = shards_[shard_id]->search(centroids, query, k);
        results.insert(results.end(), shard_result.begin(), shard_result.end());
    }
    std::sort(results.begin(), results.end());
    results.resize(k);
    return results;
}

std::vector<float> DistributedIndexIVF::sample_training_vectors(const std::vector<float>& vectors, int64_t n_train) const {
    const int64_t total_vectors = vectors.size() / dimension_;
    const int64_t actual_n_train = std::min(n_train, total_vectors);
    
    // Pre-allocate result vector
    std::vector<float> train_vectors;
    train_vectors.reserve(actual_n_train * dimension_);
    
    // Use reservoir sampling for better performance on large datasets
    std::random_device rd;
    std::mt19937 gen(rd());
    
    if (actual_n_train >= total_vectors) {
        // If we need all vectors, just copy them
        train_vectors = vectors;
    } else {
        // Reservoir sampling algorithm - O(N) time, O(K) space
        std::vector<int64_t> reservoir(actual_n_train);
        
        // Initialize reservoir with first K elements
        for (int64_t i = 0; i < actual_n_train; ++i) {
            reservoir[i] = i;
        }
        
        // Replace elements with decreasing probability
        for (int64_t i = actual_n_train; i < total_vectors; ++i) {
            std::uniform_int_distribution<int64_t> dist(0, i);
            int64_t j = dist(gen);
            if (j < actual_n_train) {
                reservoir[j] = i;
            }
        }
        
        // Extract selected vectors
        for (int64_t idx : reservoir) {
            train_vectors.insert(train_vectors.end(),
                               vectors.begin() + idx * dimension_,
                               vectors.begin() + (idx + 1) * dimension_);
        }
    }
    
    return train_vectors;
}

int64_t DistributedIndexIVF::find_closest_optimized(const float* x, const float* y, int d, int n) const {
    float min_dis = std::numeric_limits<float>::max();
    int64_t r = 0;
    
    // Unroll loop for better performance when d is small and known
    if (d == 4) {
        for (int64_t i = 0; i < n; i++) {
            const float* centroid = x + i * d;
            float dx = centroid[0] - y[0];
            float dy = centroid[1] - y[1];
            float dz = centroid[2] - y[2];
            float dw = centroid[3] - y[3];
            float dis = dx*dx + dy*dy + dz*dz + dw*dw;
            
            if (dis < min_dis) {
                min_dis = dis;
                r = i;
            }
        }
    } else if (d == 8) {
        for (int64_t i = 0; i < n; i++) {
            const float* centroid = x + i * d;
            float dis = 0.0f;
            for (int j = 0; j < 8; j += 4) {
                float dx = centroid[j] - y[j];
                float dy = centroid[j+1] - y[j+1];
                float dz = centroid[j+2] - y[j+2];
                float dw = centroid[j+3] - y[j+3];
                dis += dx*dx + dy*dy + dz*dz + dw*dw;
            }
            
            if (dis < min_dis) {
                min_dis = dis;
                r = i;
            }
        }
    } else {
        // General case - use original implementation
        for (int64_t i = 0; i < n; i++) {
            float dis = L2_distance(x + i * d, y, d);
            if (dis < min_dis) {
                min_dis = dis;
                r = i;
            }
        }
    }
    
    return r;
}
}

