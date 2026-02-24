//
// Created by skyitachi on 2026/2/23.
//
#include <gtest/gtest.h>
#include "dann/clustering.h"

#include <random>

class ClusteringTest: public ::testing::Test {
protected:
  void SetUp() override {

  }
  void generate_test_data() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0f, 1.0f);

    const size_t num_vectors = 65536;
    test_vectors_.clear();
    test_ids_.clear();

    for (size_t i = 0; i < num_vectors; ++i) {
      std::vector<float> vector(64);
      for (int j = 0; j < 64; ++j) {
        vector[j] = dist(gen);
      }

      test_vectors_.insert(test_vectors_.end(), vector.begin(), vector.end());
      test_ids_.push_back(static_cast<int64_t>(i));
    }
  }

  std::vector<float> test_vectors_;
  std::vector<faiss::idx_t> test_ids_;

};

TEST_F(ClusteringTest, Basic) {
  generate_test_data();
  int d = 64;
  int k = 10;
  dann::Clustering clustering(d, k);

  clustering.train(test_vectors_, test_ids_);
  ASSERT_EQ(k, clustering.centroids.size() / d);
}

TEST_F(ClusteringTest, SimpleDataset) {
  // 创建明显的聚类数据
  std::vector<float> simple_data = {
    // 聚类1: (0,0) 附近
    0.1f, 0.1f, 0.2f, 0.0f,
    // 聚类2: (10,10) 附近
    9.8f, 10.1f, 10.2f, 9.9f
  };
  std::vector<faiss::idx_t> simple_ids = {1, 2, 3, 4};

  dann::Clustering clustering(2, 2);
  clustering.train(simple_data, simple_ids);

  // 验证质心接近预期位置
  EXPECT_NEAR(clustering.centroids[0], 0.1f, 0.5f);
  EXPECT_NEAR(clustering.centroids[1], 0.1f, 0.5f);
  EXPECT_NEAR(clustering.centroids[2], 10.0f, 0.5f);
  EXPECT_NEAR(clustering.centroids[3], 10.0f, 0.5f);
}