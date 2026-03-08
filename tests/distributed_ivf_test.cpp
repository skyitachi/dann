//
// Created by skyitachi on 2026/3/8.
//
#include <gtest/gtest.h>
#include "dann/ivf_shard.h"
#include "dann/types.h"

#include <random>

#include "dann/logger.h"

class IndexIVFShardTest: public ::testing::Test {
protected:
  void SetUp() override {
    d_ = 64;
    shard_id_ = 0;
    node_id_ = "node_0";
  }

  void generate_test_data(int n, std::vector<float>& vectors, std::vector<int64_t>& ids) {
    std::random_device rd;
    std::mt19937 gen(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    vectors.clear();
    vectors.reserve(n * d_);
    ids.clear();
    ids.reserve(n);

    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < d_; ++j) {
        vectors.push_back(i * 1.0f + dist(gen));
      }
      ids.push_back(static_cast<int64_t>(i));
    }
  }

  int d_;
  int shard_id_;
  std::string node_id_;
};
TEST_F(IndexIVFShardTest, BasicSearch) {
  int d = d_;
  dann::IndexIVFShard shard(d, shard_id_, node_id_);

  std::vector<float> vectors;
  std::vector<int64_t> ids;
  generate_test_data(100, vectors, ids);

  dann::InvertedList posting;
  posting.vectors = vectors;
  posting.vector_ids = ids;

  shard.add_posting(0, posting);

  std::vector<int64_t> centroid_ids = {0};
  std::vector<float> query(d, 0.0f);
  for (int i = 0; i < d; ++i) {
    query[i] = vectors[i];
  }

  int k = 10;
  auto results = shard.search(centroid_ids, query, k);
  ASSERT_EQ(results.size(), k);
  EXPECT_EQ(results[0].id, 0);
}

TEST_F(IndexIVFShardTest, EmptyCentroidsReturnsEmptyResult) {
  dann::IndexIVFShard shard(d_, shard_id_, node_id_);

  std::vector<float> vectors;
  std::vector<int64_t> ids;
  generate_test_data(10, vectors, ids);

  dann::InvertedList posting;
  posting.vectors = vectors;
  posting.vector_ids = ids;
  shard.add_posting(0, posting);

  std::vector<int64_t> empty_centroids;
  std::vector<float> query(d_, 0.0f);

  auto results = shard.search(empty_centroids, query, 10);
  EXPECT_TRUE(results.empty());
}

TEST_F(IndexIVFShardTest, NonExistentCentroidReturnsEmptyResult) {
  dann::IndexIVFShard shard(d_, shard_id_, node_id_);

  std::vector<float> vectors;
  std::vector<int64_t> ids;
  generate_test_data(10, vectors, ids);

  dann::InvertedList posting;
  posting.vectors = vectors;
  posting.vector_ids = ids;
  shard.add_posting(0, posting);

  std::vector<int64_t> centroid_ids = {999};
  std::vector<float> query(d_, 0.0f);

  auto results = shard.search(centroid_ids, query, 10);
  EXPECT_TRUE(results.empty());
}

TEST_F(IndexIVFShardTest, MultipleCentroidsSearch) {
  dann::IndexIVFShard shard(d_, shard_id_, node_id_);

  // Add posting for centroid 0
  std::vector<float> vectors0;
  std::vector<int64_t> ids0;
  generate_test_data(50, vectors0, ids0);
  dann::InvertedList posting0;
  posting0.vectors = vectors0;
  posting0.vector_ids = ids0;
  shard.add_posting(0, posting0);

  // Add posting for centroid 1
  std::vector<float> vectors1;
  std::vector<int64_t> ids1;
  generate_test_data(50, vectors1, ids1);
  dann::InvertedList posting1;
  posting1.vectors = vectors1;
  posting1.vector_ids = ids1;
  for (auto& id : posting1.vector_ids) {
    id += 1000;
  }
  shard.add_posting(1, posting1);

  std::vector<int64_t> centroid_ids = {0, 1};
  std::vector<float> query(d_, 0.0f);
  for (int i = 0; i < d_; ++i) {
    query[i] = vectors0[i];
  }

  auto results = shard.search(centroid_ids, query, 10);
  EXPECT_EQ(results.size(), 10);
}

TEST_F(IndexIVFShardTest, SearchWithKEquals1) {
  dann::IndexIVFShard shard(d_, shard_id_, node_id_);

  std::vector<float> vectors;
  std::vector<int64_t> ids;
  generate_test_data(100, vectors, ids);

  dann::InvertedList posting;
  posting.vectors = vectors;
  posting.vector_ids = ids;
  shard.add_posting(0, posting);

  std::vector<int64_t> centroid_ids = {0};
  std::vector<float> query(d_, 0.0f);
  for (int i = 0; i < d_; ++i) {
    query[i] = vectors[i];
  }

  auto results = shard.search(centroid_ids, query, 1);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].id, 0);
}

TEST_F(IndexIVFShardTest, SearchReturnsCorrectVector) {
  dann::IndexIVFShard shard(d_, shard_id_, node_id_);

  std::vector<float> vectors;
  std::vector<int64_t> ids;
  generate_test_data(10, vectors, ids);

  dann::InvertedList posting;
  posting.vectors = vectors;
  posting.vector_ids = ids;
  shard.add_posting(0, posting);

  std::vector<int64_t> centroid_ids = {0};
  std::vector<float> query(d_, 0.0f);
  for (int i = 0; i < d_; ++i) {
    query[i] = vectors[i];
  }

  auto results = shard.search(centroid_ids, query, 5);
  ASSERT_EQ(results.size(), 5);
  EXPECT_EQ(results[0].id, 0);
  EXPECT_EQ(results[0].vector.size(), static_cast<size_t>(d_));
}

TEST_F(IndexIVFShardTest, SearchResultsSortedByDistance) {
  dann::IndexIVFShard shard(2, shard_id_, node_id_);

  // Simple 2D vectors for easier distance calculation
  std::vector<float> vectors = {
    0.0f, 0.0f,  // id 0: distance from (1,1) = 2
    1.0f, 1.0f,  // id 1: distance from (1,1) = 0 (exact match)
    2.0f, 2.0f,  // id 2: distance from (1,1) = 2
    3.0f, 3.0f,  // id 3: distance from (1,1) = 8
    4.0f, 4.0f   // id 4: distance from (1,1) = 18
  };

  std::vector<int64_t> ids = {0, 1, 2, 3, 4};

  dann::InvertedList posting;
  posting.vectors = vectors;
  posting.vector_ids = ids;
  shard.add_posting(0, posting);

  std::vector<int64_t> centroid_ids = {0};
  std::vector<float> query = {1.0f, 1.0f};

  auto results = shard.search(centroid_ids, query, 3);
  ASSERT_EQ(results.size(), 3);

  // Results should be sorted by distance ascending
  EXPECT_EQ(results[0].id, 1);  // distance 0
  EXPECT_EQ(results[1].id, 0);  // distance 2
  EXPECT_EQ(results[2].id, 2);  // distance 2

  // Verify distances are non-decreasing
  for (size_t i = 1; i < results.size(); ++i) {
    EXPECT_LE(results[i-1].distance, results[i].distance);
  }
}

TEST_F(IndexIVFShardTest, SearchWithKGreaterThanDataSize) {
  dann::IndexIVFShard shard(2, shard_id_, node_id_);

  std::vector<float> vectors = {
    0.0f, 0.0f,
    1.0f, 1.0f,
    2.0f, 2.0f
  };
  std::vector<int64_t> ids = {0, 1, 2};

  dann::InvertedList posting;
  posting.vectors = vectors;
  posting.vector_ids = ids;
  shard.add_posting(0, posting);

  std::vector<int64_t> centroid_ids = {0};
  std::vector<float> query = {0.0f, 0.0f};

  auto results = shard.search(centroid_ids, query, 10);
  EXPECT_EQ(results.size(), 3);  // Should return all 3, not 10
}
