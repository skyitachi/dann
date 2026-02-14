//
// Created by skyitachi on 2026/2/8.
//

#include "dann/clustering.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace dann {
void Clustering::train(faiss::idx_t n, const float* x, faiss::Index& index, const float* x_weights) {

}

void Clustering::train(faiss::idx_t d, const std::vector<float>& vectors, const std::vector<faiss::idx_t>& ids) {
    std::vector<faiss::idx_t> local_indices(ids.size());
    std::iota(local_indices.begin(), local_indices.end(), static_cast<faiss::idx_t>(0));

    std::mt19937_64 rng(static_cast<uint64_t>(seed));
    std::shuffle(local_indices.begin(), local_indices.end(), rng);

    // 取random_sample_count数量的向量做k-means
    uint64_t random_sample_count = get_sample_count(ids.size());


}

uint64_t Clustering::get_sample_count(faiss::idx_t n) {
    uint64_t lo = static_cast<uint64_t>(min_points_per_centroids);
    uint64_t hi = static_cast<uint64_t>(max_points_per_centroids);
    if (lo > hi) {
        std::swap(lo, hi);
    }

    std::mt19937_64 rng(static_cast<uint64_t>(seed));
    std::uniform_int_distribution<uint64_t> dist(lo, hi);
    const uint64_t points_per_centroid = dist(rng);

    const uint64_t k_u64 = static_cast<uint64_t>(k);
    const uint64_t n_u64 = static_cast<uint64_t>(n);

    const uint64_t target = k_u64 * points_per_centroid;
    const uint64_t ratio_cap = static_cast<uint64_t>(std::floor(static_cast<double>(max_sample_ratio) * static_cast<double>(n_u64)));

    uint64_t result = std::min({n_u64, target, ratio_cap});
    result = std::max(result, k_u64);
    return result;
}
}
