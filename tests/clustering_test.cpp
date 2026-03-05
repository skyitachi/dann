//
// Created by skyitachi on 2026/2/23.
//
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "dann/clustering.h"
#include "dann/utils.h"

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

TEST(ClusteringUtilsTest, L2Distance) {
  const float a[] = {1.0f, 2.0f};
  const float b[] = {4.0f, 6.0f};

  const float dist = dann::L2_distance(a, b, 2);
  EXPECT_FLOAT_EQ(dist, 25.0f);

  {
    const float a[] = {9.8f, 10.1f};
    const float b[] = {0.1f, 0.1f};

    const float dist = dann::L2_distance(a, b, 2);
    EXPECT_NEAR(dist, 194.09f, 1e-4f);
  }
}

TEST(ClusteringUtilsTest, FindClosestBasic) {
  // Test with simple 2D vectors
  std::vector<float> vectors = {
    1.0f, 1.0f,  // vector 0
    5.0f, 5.0f,  // vector 1
    3.0f, 3.0f,  // vector 2
    10.0f, 10.0f // vector 3
  };
  
  const float query[] = {2.5f, 2.5f};
  int d = 2;
  int n = 4;
  
  int64_t closest_idx = dann::find_closest(vectors.data(), query, d, n);
  EXPECT_EQ(closest_idx, 2); // vector 2 (3,3) should be closest to (2.5,2.5)
}

TEST(ClusteringUtilsTest, FindClosestExactMatch) {
  std::vector<float> vectors = {
    1.0f, 2.0f, 3.0f,
    4.0f, 5.0f, 6.0f,
    7.0f, 8.0f, 9.0f
  };
  
  const float query[] = {4.0f, 5.0f, 6.0f}; // Exact match with vector 1
  int d = 3;
  int n = 3;
  
  int64_t closest_idx = dann::find_closest(vectors.data(), query, d, n);
  EXPECT_EQ(closest_idx, 1);
}

TEST(ClusteringUtilsTest, FindClosestSingleVector) {
  std::vector<float> vectors = {1.0f, 2.0f, 3.0f};
  const float query[] = {10.0f, 20.0f, 30.0f};
  int d = 3;
  int n = 1;
  
  int64_t closest_idx = dann::find_closest(vectors.data(), query, d, n);
  EXPECT_EQ(closest_idx, 0);
}

TEST(ClusteringUtilsTest, FindClosestHighDimensional) {
  // Test with 128-dimensional vectors
  const int d = 128;
  const int n = 5;
  std::vector<float> vectors(n * d, 0.0f);
  
  // Create vectors with increasing values
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < d; ++j) {
      vectors[i * d + j] = static_cast<float>(i * 10 + j);
    }
  }
  
  std::vector<float> query(d, 25.0f); // Query close to vector 2
  
  int64_t closest_idx = dann::find_closest(vectors.data(), query.data(), d, n);
  EXPECT_EQ(closest_idx, 0);
}

TEST(ClusteringUtilsTest, FindClosestNegativeValues) {
  std::vector<float> vectors = {
    -1.0f, -1.0f,  // vector 0
    -5.0f, -5.0f,  // vector 1
    0.0f, 0.0f,    // vector 2
    1.0f, 1.0f     // vector 3
  };
  
  const float query[] = {-0.3f, -0.3f};
  int d = 2;
  int n = 4;
  
  int64_t closest_idx = dann::find_closest(vectors.data(), query, d, n);
  EXPECT_EQ(closest_idx, 2); // vector 2 (0,0) should be closest
}

TEST(ClusteringUtilsTest, FindClosestTieBreaking) {
  // Test case where two vectors are equidistant
  std::vector<float> vectors = {
    1.0f, 0.0f,  // vector 0: distance = 1
    -1.0f, 0.0f, // vector 1: distance = 1
    2.0f, 0.0f   // vector 2: distance = 4
  };
  
  const float query[] = {0.0f, 0.0f};
  int d = 2;
  int n = 3;
  
  int64_t closest_idx = dann::find_closest(vectors.data(), query, d, n);
  // Should return the first one found (index 0) due to < comparison
  EXPECT_EQ(closest_idx, 0);
}

TEST(ClusteringUtilsTest, FindClosestKBasic) {
  std::vector<float> vectors = {
    1.0f, 1.0f,  // vector 0: distance = 0.5
    5.0f, 5.0f,  // vector 1: distance = 12.5
    3.0f, 3.0f,  // vector 2: distance = 4.5
    2.0f, 2.0f,  // vector 3: distance = 0.5
    10.0f, 10.0f // vector 4: distance = 80.5
  };
  
  const float query[] = {1.5f, 1.5f};
  int d = 2;
  int n = 5;
  int k = 3;
  
  std::vector<int64_t> result = dann::find_closest_k(vectors.data(), query, d, n, k);
  EXPECT_EQ(result.size(), 3);
  // Check that we get 3 closest vectors
  EXPECT_THAT(result, testing::Contains(0));
  EXPECT_THAT(result, testing::Contains(2));
  EXPECT_THAT(result, testing::Contains(3));
}

TEST(ClusteringUtilsTest, FindClosestKSmallK) {
  std::vector<float> vectors = {
    1.0f, 1.0f,
    2.0f, 2.0f,
    3.0f, 3.0f,
    4.0f, 4.0f
  };
  
  const float query[] = {0.0f, 0.0f};
  int d = 2;
  int n = 4;
  int k = 2;
  
  std::vector<int64_t> result = dann::find_closest_k(vectors.data(), query, d, n, k);
  EXPECT_EQ(result.size(), 2);
  EXPECT_THAT(result, testing::Contains(0));
  EXPECT_THAT(result, testing::Contains(1));
}

TEST(ClusteringUtilsTest, FindClosestKAllVectors) {
  std::vector<float> vectors = {
    1.0f, 2.0f,
    3.0f, 4.0f,
    5.0f, 6.0f
  };
  
  const float query[] = {0.0f, 0.0f};
  int d = 2;
  int n = 3;
  int k = 3;
  
  std::vector<int64_t> result = dann::find_closest_k(vectors.data(), query, d, n, k);
  EXPECT_EQ(result.size(), 3);
}

TEST(ClusteringUtilsTest, FindClosestKVectorVersion) {
  std::vector<float> x = {1.5f, 1.5f};
  std::vector<float> y = {
    1.0f, 1.0f,
    5.0f, 5.0f,
    3.0f, 3.0f,
    2.0f, 2.0f
  };
  
  int d = 2;
  int n = 4;
  int k = 2;
  
  std::vector<int64_t> result = dann::find_closest_k(x, y, d, n, k);
  EXPECT_EQ(result.size(), 2);
}

TEST(ClusteringUtilsTest, FindClosestKHighDimensional) {
  const int d = 128;
  const int n = 10;
  const int k = 5;
  
  std::vector<float> vectors(n * d);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < d; ++j) {
      vectors[i * d + j] = static_cast<float>(i * 10 + j);
    }
  }
  
  std::vector<float> query(d);
  for (int i = 0; i < d; ++i) {
    query[i] = static_cast<float>(i);
  }
  
  std::vector<int64_t> result = dann::find_closest_k(vectors.data(), query.data(), d, n, k);
  EXPECT_EQ(result.size(), k);
  EXPECT_EQ(result[0], 0);
}

TEST(ClusteringUtilsTest, FindClosestKNegativeValues) {
  std::vector<float> vectors = {
    -1.0f, -1.0f,
    -5.0f, -5.0f,
    0.0f, 0.0f,
    1.0f, 1.0f
  };
  
  const float query[] = {-0.3f, -0.3f};
  int d = 2;
  int n = 4;
  int k = 2;
  
  std::vector<int64_t> result = dann::find_closest_k(vectors.data(), query, d, n, k);
  EXPECT_EQ(result.size(), 2);
  EXPECT_THAT(result, testing::Contains(2));
}