//
// Created by skyitachi on 2026/3/8.
//
#include <gtest/gtest.h>
#include "dann/distributed_index_ivf.h"
#include "dann/ivf_shard.h"
#include "dann/types.h"
#include "dann/index_persistence_manager.h"

#include <random>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

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

class DistributedIndexIVFTest : public ::testing::Test {
protected:
  void SetUp() override {
    d_ = 8;  // hit optimized distance branch in implementation
    shards_ = 4;
    nodes_ = {"node_0", "node_1"};
  }

  void generate_clustered_data(int n, std::vector<float>& vectors, std::vector<int64_t>& ids) const {
    vectors.clear();
    ids.clear();
    vectors.reserve(static_cast<size_t>(n) * d_);
    ids.reserve(static_cast<size_t>(n));

    for (int i = 0; i < n; ++i) {
      const float base = static_cast<float>(i / 10) * 20.0f;
      for (int j = 0; j < d_; ++j) {
        vectors.push_back(base + static_cast<float>(j) * 0.01f);
      }
      ids.push_back(static_cast<int64_t>(i));
    }
  }

  int d_;
  int shards_;
  std::vector<std::string> nodes_;
};

TEST_F(DistributedIndexIVFTest, BuildAndSearchReturnsResults) {
  dann::DistributedIndexIVF index("distributed_ivf", d_, shards_, nodes_);

  std::vector<float> vectors;
  std::vector<int64_t> ids;
  generate_clustered_data(200, vectors, ids);

  ASSERT_TRUE(index.add_vectors(vectors, ids));

  std::vector<float> query(vectors.begin(), vectors.begin() + d_);
  const int k = 10;
  auto results = index.search(query, k);

  ASSERT_EQ(results.size(), static_cast<size_t>(k));
  EXPECT_EQ(results[0].distance, 0.0f);
  EXPECT_GE(results[0].id, 0);
  EXPECT_LE(results[0].id, 9);
  for (size_t i = 1; i < results.size(); ++i) {
    EXPECT_LE(results[i - 1].distance, results[i].distance);
  }
}

TEST_F(DistributedIndexIVFTest, IndexMetadataIsCorrect) {
  dann::DistributedIndexIVF index("distributed_ivf_meta", d_, shards_, nodes_);
  EXPECT_EQ(index.dimension(), d_);
  EXPECT_EQ(index.index_type(), "IVF");
}

TEST_F(DistributedIndexIVFTest, LoadIndexReturnsFalse) {
  dann::DistributedIndexIVF index("distributed_ivf_load", d_, shards_, nodes_);
  EXPECT_FALSE(index.load_index("/tmp/not_implemented.ivf"));
}

class IndexIVFShardPersistenceTest : public ::testing::Test {
protected:
  void SetUp() override {
    d_ = 64;
    test_dir_ = "/tmp/dann_persistence_test_" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(test_dir_);
  }

  void TearDown() override {
    std::filesystem::remove_all(test_dir_);
  }

  void generate_test_data(int n, std::vector<float>& vectors, std::vector<int64_t>& ids) {
    std::mt19937 gen(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    vectors.clear();
    vectors.reserve(n * d_);
    ids.clear();
    ids.reserve(n);

    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < d_; ++j) {
        vectors.push_back(static_cast<float>(i) + dist(gen));
      }
      ids.push_back(static_cast<int64_t>(i));
    }
  }

  int d_;
  std::string test_dir_;
};

TEST_F(IndexIVFShardPersistenceTest, SaveAndLoadPreservesData) {
  dann::IndexIVFShard original(d_, 0, "node_0");

  std::vector<float> vectors;
  std::vector<int64_t> ids;
  generate_test_data(100, vectors, ids);

  dann::InvertedList posting;
  posting.vectors = vectors;
  posting.vector_ids = ids;
  original.add_posting(0, posting);

  std::string file_path = test_dir_ + "/test_shard.posting";
  auto status = original.Save(file_path);
  ASSERT_TRUE(status.ok()) << status.ToString();

  dann::IndexIVFShard loaded(d_, 0, "node_0");
  status = loaded.Load(file_path);
  ASSERT_TRUE(status.ok()) << status.ToString();

  EXPECT_EQ(loaded.dimension(), d_);
  EXPECT_EQ(loaded.num_postings(), original.num_postings());
  EXPECT_EQ(loaded.total_vectors(), original.total_vectors());
}

TEST_F(IndexIVFShardPersistenceTest, LoadInvalidFileReturnsError) {
  dann::IndexIVFShard shard(d_, 0, "node_0");
  
  auto status = shard.Load("/nonexistent/path/file.posting");
  EXPECT_FALSE(status.ok());
}

TEST_F(IndexIVFShardPersistenceTest, LoadCorruptedFileReturnsError) {
  std::string corrupt_file = test_dir_ + "/corrupt.posting";
  std::ofstream ofs(corrupt_file, std::ios::binary);
  ofs << "invalid binary content";
  ofs.close();

  dann::IndexIVFShard shard(d_, 0, "node_0");
  auto status = shard.Load(corrupt_file);
  EXPECT_FALSE(status.ok());
}

TEST_F(IndexIVFShardPersistenceTest, SaveCreatesParentDirectories) {
  dann::IndexIVFShard shard(d_, 0, "node_0");
  
  std::vector<float> vectors;
  std::vector<int64_t> ids;
  generate_test_data(10, vectors, ids);

  dann::InvertedList posting;
  posting.vectors = vectors;
  posting.vector_ids = ids;
  shard.add_posting(0, posting);

  std::string nested_path = test_dir_ + "/nested/deep/path/file.posting";
  auto status = shard.Save(nested_path);
  EXPECT_TRUE(status.ok()) << status.ToString();
  EXPECT_TRUE(std::filesystem::exists(nested_path));
}

class DistributedIndexIVFPersistenceTest : public ::testing::Test {
protected:
  void SetUp() override {
    d_ = 8;
    shards_ = 2;
    nodes_ = {"node_0", "node_1"};
    test_dir_ = "/tmp/dann_index_persistence_test_" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(test_dir_);
  }

  void TearDown() override {
    std::filesystem::remove_all(test_dir_);
  }

  void generate_clustered_data(int n, std::vector<float>& vectors, std::vector<int64_t>& ids) {
    vectors.clear();
    ids.clear();
    vectors.reserve(n * d_);
    ids.reserve(n);

    for (int i = 0; i < n; ++i) {
      const float base = static_cast<float>(i / 10) * 20.0f;
      for (int j = 0; j < d_; ++j) {
        vectors.push_back(base + static_cast<float>(j) * 0.01f);
      }
      ids.push_back(static_cast<int64_t>(i));
    }
  }

  int d_;
  int shards_;
  std::vector<std::string> nodes_;
  std::string test_dir_;
};

TEST_F(DistributedIndexIVFPersistenceTest, SaveAndLoadRoundTrip) {
  auto original = std::make_unique<dann::DistributedIndexIVF>(
      "test_index", d_, shards_, nodes_);

  std::vector<float> vectors;
  std::vector<int64_t> ids;
  generate_clustered_data(100, vectors, ids);

  ASSERT_TRUE(original->add_vectors(vectors, ids));
  EXPECT_TRUE(original->is_dirty());

  std::vector<float> query(vectors.begin(), vectors.begin() + d_);
  auto original_results = original->search(query, 5);

  std::string index_path = test_dir_ + "/test_index";
  EXPECT_TRUE(original->save_index(index_path));
  EXPECT_FALSE(original->is_dirty());

  auto loaded = std::make_unique<dann::DistributedIndexIVF>(
      "test_index", d_, shards_, nodes_);
  EXPECT_TRUE(loaded->load_index(index_path));
  EXPECT_FALSE(loaded->is_dirty());

  auto loaded_results = loaded->search(query, 5);
  
  ASSERT_EQ(original_results.size(), loaded_results.size());
  for (size_t i = 0; i < original_results.size(); ++i) {
    EXPECT_EQ(original_results[i].id, loaded_results[i].id);
  }
}

TEST_F(DistributedIndexIVFPersistenceTest, LoadNonexistentIndexReturnsFalse) {
  dann::DistributedIndexIVF index("nonexistent", d_, shards_, nodes_);
  EXPECT_FALSE(index.load_index("/nonexistent/path/index"));
}

TEST_F(DistributedIndexIVFPersistenceTest, DirtyFlagTracking) {
  dann::DistributedIndexIVF index("dirty_test", d_, shards_, nodes_);
  
  EXPECT_FALSE(index.is_dirty());

  std::vector<float> vectors;
  std::vector<int64_t> ids;
  generate_clustered_data(50, vectors, ids);
  
  index.add_vectors(vectors, ids);
  EXPECT_TRUE(index.is_dirty());

  std::string index_path = test_dir_ + "/dirty_test";
  index.save_index(index_path);
  EXPECT_FALSE(index.is_dirty());

  index.set_dirty(true);
  EXPECT_TRUE(index.is_dirty());
}

class IndexPersistenceManagerTest : public ::testing::Test {
protected:
  void SetUp() override {
    d_ = 8;
    test_dir_ = "/tmp/dann_persistence_mgr_test_" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(test_dir_);
  }

  void TearDown() override {
    std::filesystem::remove_all(test_dir_);
  }

  int d_;
  std::string test_dir_;
};

TEST_F(IndexPersistenceManagerTest, StartStopWithoutSave) {
  std::vector<std::string> nodes = {"node_0"};
  auto index = std::make_unique<dann::DistributedIndexIVF>("test", d_, 1, nodes);

  dann::PersistenceConfig config;
  config.enabled = false;
  
  dann::IndexPersistenceManager manager(index.get(), config);
  manager.start();
  EXPECT_FALSE(manager.is_running());
  
  manager.stop();
}

TEST_F(IndexPersistenceManagerTest, ForceSave) {
  std::vector<std::string> nodes = {"node_0"};
  auto index = std::make_unique<dann::DistributedIndexIVF>("force_test", d_, 1, nodes);

  std::vector<float> vectors;
  std::vector<int64_t> ids;
  for (int i = 0; i < 50; ++i) {
    for (int j = 0; j < d_; ++j) {
      vectors.push_back(static_cast<float>(i));
    }
    ids.push_back(i);
  }
  index->add_vectors(vectors, ids);

  dann::PersistenceConfig config;
  config.enabled = true;
  config.save_interval_seconds = 3600; 
  config.index_path = test_dir_;

  dann::IndexPersistenceManager manager(index.get(), config);
  manager.start();
  EXPECT_TRUE(manager.is_running());

  manager.force_save();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  manager.stop();
  EXPECT_FALSE(manager.is_running());
}
