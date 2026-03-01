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

DistributedIndexIVF::DistributedIndexIVF(std::string name, int d, int64_t n):
    name_(std::move(name)), dimension_(d), ntotal_(n), is_trained_(false) {
    clustering_ = std::make_unique<Clustering>(d, get_nlist(n));
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

    // 2. 构建postings
    std::unordered_map<int64_t, InvertedList> postings;
    for (int i = 0; i < ids.size(); i++) {
        int64_t centroid = find_closest(&global_centroids_[0], &vectors[i * dimension_], dimension_, global_centroid_ids_.size());
        auto it = postings.find(centroid);
        if (it == postings.end()) {
            postings[centroid] = InvertedList();
            it = postings.find(centroid);
        }
        it->second.vectors.insert(it->second.vectors.end(), vectors.begin() + i * dimension_, vectors.begin() + (i + 1) * dimension_);
        it->second.vector_ids.push_back(ids.at(i));
    }
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
}

