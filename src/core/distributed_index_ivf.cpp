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
        shards_[i] = std::make_unique<IndexIVFShard>(i, nodes_[i % node_size]);
    }
}

void DistributedIndexIVF::build_index_optimized_by_swe1_5(const std::vector<float>& vectors, const std::vector<int64_t>& ids) {
    assert(dimension_ != 0);
    assert(vectors.size() / dimension_ == ids.size());
    
    const int64_t num_vectors = ids.size();
    const int64_t n_train = std::min(static_cast<int64_t>(clustering_->k) * 64, num_vectors);
    
    // 1. Optimized training vector sampling with pre-allocation
    std::vector<float> train_vectors = sample_training_vectors_optimized(vectors, n_train);
    const int64_t actual_n_train = train_vectors.size() / dimension_;
    
    // 2. Train clustering
    clustering_->train(train_vectors, actual_n_train);
    global_centroids_ = clustering_->centroids;
    const int64_t num_centroids = global_centroids_.size() / dimension_;
    
    // Pre-allocate centroid IDs
    global_centroid_ids_.resize(num_centroids);
    std::iota(global_centroid_ids_.begin(), global_centroid_ids_.end(), 0);
    
    // 3. Optimized posting construction with pre-allocation and batch processing
    std::vector<std::vector<int64_t>> centroid_to_vectors(num_centroids);
    std::vector<std::vector<int64_t>> centroid_to_ids(num_centroids);
    
    // Pre-allocate space for each centroid based on expected load
    const int64_t avg_vectors_per_centroid = num_vectors / num_centroids + 1;
    for (int64_t i = 0; i < num_centroids; ++i) {
        centroid_to_vectors[i].reserve(avg_vectors_per_centroid);
        centroid_to_ids[i].reserve(avg_vectors_per_centroid);
    }
    
    // Batch assign vectors to centroids
    for (int64_t i = 0; i < num_vectors; ++i) {
        const int64_t centroid = find_closest_optimized(
            global_centroids_.data(), 
            vectors.data() + i * dimension_, 
            dimension_, 
            num_centroids
        );
        centroid_to_vectors[centroid].push_back(i);
        centroid_to_ids[centroid].push_back(ids[i]);
    }
    
    // 4. Optimized shard distribution with reduced memory copies
    for (int64_t centroid = 0; centroid < num_centroids; ++centroid) {
        if (centroid_to_vectors[centroid].empty()) continue;
        
        const int shard_id = centroid % shard_counts_;
        
        // Create inverted list with pre-allocated capacity
        InvertedList inv_list;
        const size_t list_size = centroid_to_vectors[centroid].size();
        inv_list.vectors.reserve(list_size * dimension_);
        inv_list.vector_ids.reserve(list_size);
        
        // Batch copy vectors and IDs
        for (size_t i = 0; i < list_size; ++i) {
            const int64_t vector_idx = centroid_to_vectors[centroid][i];
            inv_list.vectors.insert(inv_list.vectors.end(),
                                  vectors.begin() + vector_idx * dimension_,
                                  vectors.begin() + (vector_idx + 1) * dimension_);
            inv_list.vector_ids.push_back(centroid_to_ids[centroid][i]);
        }
        
        shards_[shard_id]->add_posting(centroid, std::move(inv_list));
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

std::vector<float> DistributedIndexIVF::sample_training_vectors(const std::vector<float>& vectors, int64_t n_train) const {
    // 从vectors中随机选出n_train的d维向量
    int64_t total_vectors = vectors.size() / dimension_;
    int64_t actual_n_train = std::min(n_train, total_vectors);
    
    // 创建随机索引
    std::vector<int64_t> indices(total_vectors);
    std::iota(indices.begin(), indices.end(), 0);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices.begin(), indices.end(), gen);
    
    // 提取随机选择的向量
    std::vector<float> train_vectors;
    train_vectors.reserve(actual_n_train * dimension_);
    for (int64_t i = 0; i < actual_n_train; i++) {
        int64_t idx = indices[i];
        train_vectors.insert(train_vectors.end(), 
                           vectors.begin() + idx * dimension_, 
                           vectors.begin() + (idx + 1) * dimension_);
    }
    
    return train_vectors;
}

std::vector<InternalSearchResult> DistributedIndexIVF::search(const std::vector<float>& query, int k, int nprobe) {
    if (nprobe > global_centroid_ids_.size()) {
        nprobe = global_centroid_ids_.size();
    }
    // 从global_vectors中找到nprobe和query最近的向量

}

std::vector<float> DistributedIndexIVF::sample_training_vectors_optimized(const std::vector<float>& vectors, int64_t n_train) const {
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

