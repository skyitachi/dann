//
// Created by skyitachi on 2026/2/8.
//

#ifndef DANN_CLUSTERING_H
#define DANN_CLUSTERING_H
#include "dann/types.h"
#include <faiss/Index.h>
#include <faiss/MetricType.h>

namespace dann {

struct ClusteringParameters {
    int niter = 25;
    bool int_centroids = false;
    int min_points_per_centroids = 39;
    int max_points_per_centroids = 256;
    float max_sample_ratio = 0.22;
    int seed = 1234;
};

struct Clustering:ClusteringParameters {
    size_t d; // dimension
    size_t k; // number of centroids

    std::vector<float> centroids;

    Clustering(int d, int k);
    Clustering(int d, int k, const ClusteringParameters& cp);

     void train(
            faiss::idx_t n,
            const float* x,
            faiss::Index& index,
            const float* x_weights = nullptr);

    void train(faiss::idx_t d, const std::vector<float>& vectors, const std::vector<faiss::idx_t>& ids);

    virtual ~Clustering() {}

private:
    uint64_t get_sample_count(faiss::idx_t n);

};
/** simplified interface
 *
 * @param d dimension of the data
 * @param n nb of training vectors
 * @param k nb of output centroids
 * @param x training set (size n * d)
 * @param centroids output centroids (size k * d)
 * @return final quantization error
 */
float kmeans_clustering(
        size_t d,
        size_t n,
        size_t k,
        const float* x,
        float* centroids);
}
#endif //DANN_CLUSTERING_H