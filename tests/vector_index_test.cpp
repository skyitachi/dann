#include "dann/vector_index.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <random>
#include <chrono>
#include <fstream>
#include <filesystem>

using namespace dann;

class VectorIndexTest : public ::testing::Test {
protected:
    void SetUp() override {
        dimension_ = 128;
        index_type_ = "IVF";
        hnsw_m_ = 16;
        hnsw_ef_construction_ = 100;
        
        // Generate test data
        generate_test_data();
    }
    
    void TearDown() override {
        // Clean up any test files
        std::filesystem::remove("/tmp/test_vector_index.idx");
        std::filesystem::remove("/tmp/test_vector_index_save.idx");
    }
    
    void generate_test_data() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(0.0f, 1.0f);
        
        const size_t num_vectors = 100;
        test_vectors_.clear();
        test_ids_.clear();
        
        for (size_t i = 0; i < num_vectors; ++i) {
            std::vector<float> vector(dimension_);
            for (int j = 0; j < dimension_; ++j) {
                vector[j] = dist(gen);
            }
            
            test_vectors_.insert(test_vectors_.end(), vector.begin(), vector.end());
            test_ids_.push_back(static_cast<int64_t>(i));
        }
    }
    
    std::vector<float> generate_random_vector() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(0.0f, 1.0f);
        
        std::vector<float> vector(dimension_);
        for (int i = 0; i < dimension_; ++i) {
            vector[i] = dist(gen);
        }
        
        return vector;
    }
    
    std::vector<float> generate_specific_vector(float value) {
        std::vector<float> vector(dimension_, value);
        return vector;
    }
    
    int dimension_;
    std::string index_type_;
    int hnsw_m_;
    int hnsw_ef_construction_;
    
    std::vector<float> test_vectors_;
    std::vector<int64_t> test_ids_;
};

// Constructor Tests
TEST_F(VectorIndexTest, ConstructorWithDefaultParameters) {
    VectorIndex index(dimension_);
    
    EXPECT_EQ(index.dimension(), dimension_);
    EXPECT_EQ(index.index_type(), "IVF");
    EXPECT_EQ(index.size(), 0);
    EXPECT_EQ(index.get_version(), 0);
}

TEST_F(VectorIndexTest, ConstructorWithCustomParameters) {
    VectorIndex index(dimension_, "HNSW", 32, 200);
    
    EXPECT_EQ(index.dimension(), dimension_);
    EXPECT_EQ(index.index_type(), "HNSW");
    EXPECT_EQ(index.size(), 0);
    EXPECT_EQ(index.get_version(), 0);
}

TEST_F(VectorIndexTest, ConstructorWithInvalidDimension) {
    EXPECT_THROW(VectorIndex index(0), std::invalid_argument);
    EXPECT_THROW(VectorIndex index(-1), std::invalid_argument);
}

// Vector Addition Tests
TEST_F(VectorIndexTest, AddVectorsSingle) {
    VectorIndex index(dimension_);
    
    // Add a single vector
    std::vector<float> single_vector = generate_random_vector();
    std::vector<int64_t> single_id = {1};
    
    bool result = index.add_vectors(single_vector, single_id);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(index.size(), 1);
}

TEST_F(VectorIndexTest, AddVectorsMultiple) {
    VectorIndex index(dimension_);
    
    bool result = index.add_vectors(test_vectors_, test_ids_);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(index.size(), test_ids_.size());
}

TEST_F(VectorIndexTest, AddVectorsWithEmptyData) {
    VectorIndex index(dimension_);
    
    std::vector<float> empty_vectors;
    std::vector<int64_t> empty_ids;
    
    bool result = index.add_vectors(empty_vectors, empty_ids);
    
    EXPECT_FALSE(result);
    EXPECT_EQ(index.size(), 0);
}

TEST_F(VectorIndexTest, AddVectorsWithMismatchedSizes) {
    VectorIndex index(dimension_);
    
    std::vector<float> vectors(dimension_ * 5); // 5 vectors
    std::vector<int64_t> ids(3); // Only 3 IDs
    
    bool result = index.add_vectors(vectors, ids);
    
    EXPECT_FALSE(result);
    EXPECT_EQ(index.size(), 0);
}

TEST_F(VectorIndexTest, AddVectorsWithInvalidDimensions) {
    VectorIndex index(dimension_);
    
    std::vector<float> invalid_vectors(dimension_ - 1); // Wrong dimension
    std::vector<int64_t> ids = {1};
    
    bool result = index.add_vectors(invalid_vectors, ids);
    
    EXPECT_FALSE(result);
    EXPECT_EQ(index.size(), 0);
}

TEST_F(VectorIndexTest, AddVectorsBulk) {
    VectorIndex index(dimension_);
    
    bool result = index.add_vectors_bulk(test_vectors_, test_ids_, 10);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(index.size(), test_ids_.size());
}

TEST_F(VectorIndexTest, AddVectorsBulkWithInvalidBatchSize) {
    VectorIndex index(dimension_);
    
    bool result = index.add_vectors_bulk(test_vectors_, test_ids_, 0);
    
    EXPECT_FALSE(result);
    EXPECT_EQ(index.size(), 0);
}

// Search Tests
TEST_F(VectorIndexTest, SearchWithEmptyIndex) {
    VectorIndex index(dimension_);
    
    auto query_vector = generate_random_vector();
    auto results = index.search(query_vector, 5);
    
    EXPECT_TRUE(results.empty());
}

TEST_F(VectorIndexTest, SearchWithValidIndex) {
    VectorIndex index(dimension_);
    
    // Add vectors first
    ASSERT_TRUE(index.add_vectors(test_vectors_, test_ids_));
    
    auto query_vector = generate_random_vector();
    auto results = index.search(query_vector, 5);
    
    EXPECT_LE(results.size(), 5);
    EXPECT_GT(results.size(), 0);
    
    // Verify result structure
    for (const auto& result : results) {
        EXPECT_GE(result.id, 0);
        EXPECT_LT(result.id, test_ids_.size());
        EXPECT_GE(result.distance, 0.0f);
        EXPECT_EQ(result.vector.size(), dimension_);
    }
}

TEST_F(VectorIndexTest, SearchWithKGreaterThanIndexSize) {
    VectorIndex index(dimension_);
    
    // Add a few vectors
    std::vector<float> few_vectors(dimension_ * 3);
    std::vector<int64_t> few_ids = {1, 2, 3};
    ASSERT_TRUE(index.add_vectors(few_vectors, few_ids));
    
    auto query_vector = generate_random_vector();
    auto results = index.search(query_vector, 10); // k > index size
    
    EXPECT_LE(results.size(), 3);
    EXPECT_GT(results.size(), 0);
}

TEST_F(VectorIndexTest, SearchWithInvalidK) {
    VectorIndex index(dimension_);
    
    ASSERT_TRUE(index.add_vectors(test_vectors_, test_ids_));
    
    auto query_vector = generate_random_vector();
    auto results = index.search(query_vector, 0);
    
    EXPECT_TRUE(results.empty());
}

TEST_F(VectorIndexTest, SearchWithInvalidQueryDimension) {
    VectorIndex index(dimension_);
    
    ASSERT_TRUE(index.add_vectors(test_vectors_, test_ids_));
    
    std::vector<float> invalid_query(dimension_ - 1); // Wrong dimension
    auto results = index.search(invalid_query, 5);
    
    EXPECT_TRUE(results.empty());
}

TEST_F(VectorIndexTest, SearchBatch) {
    VectorIndex index(dimension_);
    
    ASSERT_TRUE(index.add_vectors(test_vectors_, test_ids_));
    
    // Create batch queries
    std::vector<float> batch_queries;
    for (int i = 0; i < 3; ++i) {
        auto query = generate_random_vector();
        batch_queries.insert(batch_queries.end(), query.begin(), query.end());
    }
    
    auto batch_results = index.search_batch(batch_queries, 5);
    
    EXPECT_EQ(batch_results.size(), 3 * 5); // 3 queries x 5 results each
}

TEST_F(VectorIndexTest, SearchBatchWithInvalidQueries) {
    VectorIndex index(dimension_);
    
    ASSERT_TRUE(index.add_vectors(test_vectors_, test_ids_));
    
    std::vector<float> invalid_queries(dimension_ - 1); // Wrong dimension
    auto batch_results = index.search_batch(invalid_queries, 5);
    
    EXPECT_TRUE(batch_results.empty());
}

// Vector Removal Tests
TEST_F(VectorIndexTest, RemoveVector) {
    VectorIndex index(dimension_);
    
    ASSERT_TRUE(index.add_vectors(test_vectors_, test_ids_));
    size_t original_size = index.size();
    
    bool result = index.remove_vector(test_ids_[0]);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(index.size(), original_size - 1);
}

TEST_F(VectorIndexTest, RemoveNonExistentVector) {
    VectorIndex index(dimension_);
    
    ASSERT_TRUE(index.add_vectors(test_vectors_, test_ids_));
    size_t original_size = index.size();
    
    bool result = index.remove_vector(999999); // Non-existent ID
    
    EXPECT_FALSE(result);
    EXPECT_EQ(index.size(), original_size);
}

TEST_F(VectorIndexTest, RemoveVectorFromEmptyIndex) {
    VectorIndex index(dimension_);
    
    bool result = index.remove_vector(1);
    
    EXPECT_FALSE(result);
    EXPECT_EQ(index.size(), 0);
}

// Vector Update Tests
TEST_F(VectorIndexTest, UpdateVector) {
    VectorIndex index(dimension_);
    
    ASSERT_TRUE(index.add_vectors(test_vectors_, test_ids_));
    
    auto new_vector = generate_random_vector();
    bool result = index.update_vector(test_ids_[0], new_vector);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(index.size(), test_ids_.size());
}

TEST_F(VectorIndexTest, UpdateNonExistentVector) {
    VectorIndex index(dimension_);
    
    ASSERT_TRUE(index.add_vectors(test_vectors_, test_ids_));
    
    auto new_vector = generate_random_vector();
    bool result = index.update_vector(999999, new_vector); // Non-existent ID
    
    EXPECT_FALSE(result);
    EXPECT_EQ(index.size(), test_ids_.size());
}

TEST_F(VectorIndexTest, UpdateVectorWithInvalidDimension) {
    VectorIndex index(dimension_);
    
    ASSERT_TRUE(index.add_vectors(test_vectors_, test_ids_));
    
    std::vector<float> invalid_vector(dimension_ - 1); // Wrong dimension
    bool result = index.update_vector(test_ids_[0], invalid_vector);
    
    EXPECT_FALSE(result);
    EXPECT_EQ(index.size(), test_ids_.size());
}

// Index Persistence Tests
TEST_F(VectorIndexTest, SaveAndLoadIndex) {
    VectorIndex index(dimension_);
    
    ASSERT_TRUE(index.add_vectors(test_vectors_, test_ids_));
    uint64_t original_size = index.size();
    uint64_t original_version = index.get_version();
    
    std::string test_file = "/tmp/test_vector_index.idx";
    
    // Save index
    bool save_result = index.save_index(test_file);
    EXPECT_TRUE(save_result);
    EXPECT_TRUE(std::filesystem::exists(test_file));
    
    // Create new index and load
    VectorIndex new_index(dimension_);
    bool load_result = new_index.load_index(test_file);
    
    EXPECT_TRUE(load_result);
    EXPECT_EQ(new_index.size(), original_size);
    EXPECT_EQ(new_index.dimension(), dimension_);
    EXPECT_EQ(new_index.index_type(), index_type_);
}

TEST_F(VectorIndexTest, SaveIndexToFileError) {
    VectorIndex index(dimension_);
    
    ASSERT_TRUE(index.add_vectors(test_vectors_, test_ids_));
    
    // Try to save to invalid path
    bool result = index.save_index("/invalid/path/test.idx");
    
    EXPECT_FALSE(result);
}

TEST_F(VectorIndexTest, LoadIndexFromFileError) {
    VectorIndex index(dimension_);
    
    // Try to load non-existent file
    bool result = index.load_index("/non/existent/file.idx");
    
    EXPECT_FALSE(result);
}

TEST_F(VectorIndexTest, ResetIndex) {
    VectorIndex index(dimension_);
    
    ASSERT_TRUE(index.add_vectors(test_vectors_, test_ids_));
    EXPECT_GT(index.size(), 0);
    
    index.reset_index();
    
    EXPECT_EQ(index.size(), 0);
    EXPECT_EQ(index.get_version(), 0);
}

// Metadata Tests
TEST_F(VectorIndexTest, MetadataAfterConstruction) {
    VectorIndex index(dimension_, "HNSW", 32, 200);
    
    EXPECT_EQ(index.dimension(), dimension_);
    EXPECT_EQ(index.index_type(), "HNSW");
    EXPECT_EQ(index.size(), 0);
}

TEST_F(VectorIndexTest, MetadataAfterAddingVectors) {
    VectorIndex index(dimension_);
    
    ASSERT_TRUE(index.add_vectors(test_vectors_, test_ids_));
    
    EXPECT_EQ(index.size(), test_ids_.size());
    EXPECT_EQ(index.dimension(), dimension_);
    EXPECT_EQ(index.index_type(), "IVF");
}

// Version Control Tests
TEST_F(VectorIndexTest, VersionControl) {
    VectorIndex index(dimension_);
    
    EXPECT_EQ(index.get_version(), 0);
    
    // Add vectors should increment version
    ASSERT_TRUE(index.add_vectors(test_vectors_, test_ids_));
    EXPECT_GT(index.get_version(), 0);
    
    uint64_t new_version = 100;
    index.set_version(new_version);
    EXPECT_EQ(index.get_version(), new_version);
}

TEST_F(VectorIndexTest, PendingOperations) {
    VectorIndex index(dimension_);
    
    auto operations = index.get_pending_operations();
    EXPECT_TRUE(operations.empty());
    
    index.clear_pending_operations();
    operations = index.get_pending_operations();
    EXPECT_TRUE(operations.empty());
}

// Edge Case Tests
TEST_F(VectorIndexTest, LargeNumberOfVectors) {
    VectorIndex index(dimension_);
    
    // Generate larger dataset
    std::vector<float> large_vectors;
    std::vector<int64_t> large_ids;
    
    const size_t large_size = 1000;
    for (size_t i = 0; i < large_size; ++i) {
        auto vector = generate_random_vector();
        large_vectors.insert(large_vectors.end(), vector.begin(), vector.end());
        large_ids.push_back(static_cast<int64_t>(i));
    }
    
    bool result = index.add_vectors_bulk(large_vectors, large_ids, 100);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(index.size(), large_size);
    
    // Test search on large dataset
    auto query_vector = generate_random_vector();
    auto results = index.search(query_vector, 10);
    
    EXPECT_LE(results.size(), 10);
    EXPECT_GT(results.size(), 0);
}

TEST_F(VectorIndexTest, IdenticalVectors) {
    VectorIndex index(dimension_);
    
    // Add identical vectors
    std::vector<float> identical_vector = generate_specific_vector(1.0f);
    std::vector<float> identical_vectors;
    std::vector<int64_t> identical_ids;
    
    for (int i = 0; i < 10; ++i) {
        identical_vectors.insert(identical_vectors.end(), 
                               identical_vector.begin(), identical_vector.end());
        identical_ids.push_back(i);
    }
    
    bool result = index.add_vectors(identical_vectors, identical_ids);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(index.size(), 10);
    
    // Search should return all identical vectors
    auto query_vector = generate_specific_vector(1.0f);
    auto results = index.search(query_vector, 5);
    
    EXPECT_LE(results.size(), 5);
    EXPECT_GT(results.size(), 0);
    
    // All results should have distance 0 (or very close)
    for (const auto& result : results) {
        EXPECT_LT(result.distance, 0.001f); // Very small distance for identical vectors
    }
}

TEST_F(VectorIndexTest, ZeroVectors) {
    VectorIndex index(dimension_);
    
    // Add zero vectors
    std::vector<float> zero_vector(dimension_, 0.0f);
    std::vector<float> zero_vectors;
    std::vector<int64_t> zero_ids;
    
    for (int i = 0; i < 5; ++i) {
        zero_vectors.insert(zero_vectors.end(), 
                           zero_vector.begin(), zero_vector.end());
        zero_ids.push_back(i);
    }
    
    bool result = index.add_vectors(zero_vectors, zero_ids);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(index.size(), 5);
    
    // Search with zero vector
    auto query_vector = std::vector<float>(dimension_, 0.0f);
    auto results = index.search(query_vector, 3);
    
    EXPECT_LE(results.size(), 3);
    EXPECT_GT(results.size(), 0);
}

// Thread Safety Tests (Basic)
TEST_F(VectorIndexTest, ConcurrentOperations) {
    VectorIndex index(dimension_);
    
    // This is a basic test - more comprehensive thread safety tests
    // would require proper synchronization setup
    ASSERT_TRUE(index.add_vectors(test_vectors_, test_ids_));
    
    auto query_vector = generate_random_vector();
    auto results = index.search(query_vector, 5);
    
    EXPECT_LE(results.size(), 5);
    EXPECT_GT(results.size(), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
