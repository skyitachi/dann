//
// Created by skyitachi on 2026/2/8.
//

#include "dann/clustering.h"
#include "dann/utils.h"
#include "dann/logger.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <limits>
#include <memory>
#include <cassert>

namespace dann {

Clustering::Clustering(int d, int k): d(d), k(k) {}

Clustering::Clustering(int d, int k, const ClusteringParameters& cp): d(d), k(k), ClusteringParameters(cp) {}

void Clustering::train(const std::vector<float>& vectors, const std::vector<faiss::idx_t>& ids) {
    train(vectors, ids.size());
}

void Clustering::train(const std::vector<float>& vectors, size_t n) {
    assert(vectors.size() / d == n);
    std::vector<faiss::idx_t> local_indices(n);
    std::iota(local_indices.begin(), local_indices.end(), static_cast<faiss::idx_t>(0));

    for (int redo = 0; redo < nredo; redo++) {
        std::mt19937_64 rng(static_cast<uint64_t>(seed));
        std::shuffle(local_indices.begin(), local_indices.end(), rng);
        centroids.resize(d * k);
        // 1. 随机设置k个质心
        for (int i = 0; i < k; i++) {
            std::copy(vectors.begin() +  local_indices[i] * d, vectors.begin() + d * local_indices[i] + d,
                centroids.begin() + i * d);
        }

        std::unique_ptr<float[]> dis(new float[n]);
        std::unique_ptr<faiss::idx_t[]> assign(new faiss::idx_t[n]);
        std::vector<float> prev_centroids(d * k);
        float convergence_threshold = 1e-6f;
        for (int t = 0; t < niter; t++) {
            prev_centroids = centroids;
            // 2.1 计算每个向量到最近的质心
            for (int i = 0; i < n; i++) {
                float min_dis = std::numeric_limits<float>::max();
                int centroid_j = 0;
                for (int j = 0; j < k; j++) {
                    float dist = L2_distance(&vectors[i * d], &centroids[j * d], d);
                    if (dist < min_dis) {
                        centroid_j = j;
                        min_dis = dist;
                    }
                }
                dis[i] = min_dis;
                assign[i] = centroid_j;
            }

            std::fill(centroids.begin(), centroids.end(), 0);
            // 2.2 重新计算质心
            std::unique_ptr<faiss::idx_t[]> counts(new faiss::idx_t[k]);
            for (int i = 0; i < k; i++) {
                counts[i] = 0;
            }
            for (int i = 0; i < n; i++) {
                counts[assign[i]] += 1;
                for (int j = 0; j < d; j++) {
                    centroids[d * assign[i] + j] += vectors[d * i + j];
                }
            }
            for (int i = 0; i < k; i++) {
                if (counts[i] == 0) {
                    continue;
                }
                for (int j = 0; j < d; j++) {
                    centroids[i * d + j] /= counts[i];
                }
            }
            // 2.3 判断误差
            if (t > 0) {
                float max_change = 0.0f;
                for (int i = 0; i < k; i++) {
                    float change = L2_distance(&prev_centroids[i * d], &centroids[i * d], d);
                    max_change = std::max(max_change, change);
                }
                if (max_change < convergence_threshold) {
                    break;
                }
            }
        }
    }
}

}
