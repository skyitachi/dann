#include "dann/vector_index.h"
#include "dann/node_manager.h"
#include "dann/consistency_manager.h"
#include "dann/query_router.h"
#include "dann/bulk_loader.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <random>
#include <chrono>

using namespace dann;

class DANNTest : public ::testing::Test {
protected:
    void SetUp() override {
        dimension_ = 128;
        index_type_ = "IVF";
        node_id_ = "test_node";
        address_ = "127.0.0.1";
        port_ = 8080;
        
        vector_index_ = std::make_shared<VectorIndex>(dimension_, index_type_);
        node_manager_ = std::make_shared<NodeManager>(node_id_, address_, port_);
        consistency_manager_ = std::make_shared<ConsistencyManager>(node_id_);
        query_router_ = std::make_shared<QueryRouter>(node_manager_);
        bulk_loader_ = std::make_shared<BulkLoader>(vector_index_, consistency_manager_);
        
        // Generate test data
        generate_test_data();
    }
    
    void TearDown() override {
        if (node_manager_->is_running()) {
            node_manager_->stop();
        }
        consistency_manager_->stop_anti_entropy();
    }
    
    void generate_test_data() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(0.0f, 1.0f);
        
        const size_t num_vectors = 1000;
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
    
    int dimension_;
    std::string index_type_;
    std::string node_id_;
    std::string address_;
    int port_;
    
    std::shared_ptr<VectorIndex> vector_index_;
    std::shared_ptr<NodeManager> node_manager_;
    std::shared_ptr<ConsistencyManager> consistency_manager_;
    std::shared_ptr<QueryRouter> query_router_;
    std::shared_ptr<BulkLoader> bulk_loader_;
    
    std::vector<float> test_vectors_;
    std::vector<int64_t> test_ids_;
};

TEST_F(DANNTest, VectorIndexBasicOperations) {
    // Test adding vectors
    EXPECT_TRUE(vector_index_->add_vectors(test_vectors_, test_ids_));
    EXPECT_EQ(vector_index_->size(), test_ids_.size());
    
    // Test search
    auto query_vector = generate_random_vector();
    auto results = vector_index_->search(query_vector, 10);
    
    EXPECT_LE(results.size(), 10);
    EXPECT_GT(results.size(), 0);
    
    // Verify result structure
    for (const auto& result : results) {
        EXPECT_GE(result.id, 0);
        EXPECT_LT(result.id, test_ids_.size());
        EXPECT_GE(result.distance, 0.0f);
    }
}

TEST_F(DANNTest, VectorIndexBatchOperations) {
    // Test bulk loading
    EXPECT_TRUE(vector_index_->add_vectors_bulk(test_vectors_, test_ids_, 100));
    EXPECT_EQ(vector_index_->size(), test_ids_.size());
    
    // Test batch search
    std::vector<float> batch_queries;
    for (int i = 0; i < 5; ++i) {
        auto query = generate_random_vector();
        batch_queries.insert(batch_queries.end(), query.begin(), query.end());
    }
    
    auto batch_results = vector_index_->search_batch(batch_queries, 5);
    EXPECT_EQ(batch_results.size(), 5 * 5); // 5 queries x 5 results each
}

TEST_F(DANNTest, VectorIndexPersistence) {
    // Add vectors
    vector_index_->add_vectors(test_vectors_, test_ids_);
    uint64_t original_size = vector_index_->size();
    uint64_t original_version = vector_index_->get_version();
    
    // Save index
    std::string test_file = "/tmp/test_dann_index.idx";
    EXPECT_TRUE(vector_index_->save_index(test_file));
    
    // Reset and load
    vector_index_->reset_index();
    EXPECT_EQ(vector_index_->size(), 0);
    
    EXPECT_TRUE(vector_index_->load_index(test_file));
    EXPECT_EQ(vector_index_->size(), original_size);
    EXPECT_GT(vector_index_->get_version(), original_version);
    
    // Cleanup
    std::remove(test_file.c_str());
}

TEST_F(DANNTest, NodeManagerBasicOperations) {
    // Test starting node manager
    EXPECT_TRUE(node_manager_->start());
    EXPECT_TRUE(node_manager_->is_running());
    
    // Test shard assignment
    std::vector<int> shards = {0, 1, 2};
    node_manager_->assign_shards(shards);
    
    auto assigned_shards = node_manager_->get_assigned_shards();
    EXPECT_EQ(assigned_shards.size(), shards.size());
    
    for (int shard : shards) {
        EXPECT_NE(std::find(assigned_shards.begin(), assigned_shards.end(), shard), 
                  assigned_shards.end());
    }
    
    // Test node registration
    NodeInfo test_node("test_node_2", "127.0.0.1", 8081);
    node_manager_->register_node(test_node);
    
    auto cluster_nodes = node_manager_->get_cluster_nodes();
    EXPECT_GE(cluster_nodes.size(), 1);
    
    // Test stopping node manager
    EXPECT_TRUE(node_manager_->stop());
    EXPECT_FALSE(node_manager_->is_running());
}

TEST_F(DANNTest, ConsistencyManagerBasicOperations) {
    // Test operation propagation
    IndexOperation add_op(IndexOperation::ADD, 1, generate_random_vector(), 
                         std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch()).count(), 1);
    
    EXPECT_TRUE(consistency_manager_->propagate_operation(add_op));
    EXPECT_TRUE(consistency_manager_->apply_operation(add_op));
    
    // Test version management
    EXPECT_EQ(consistency_manager_->get_vector_version(1), 1);
    
    consistency_manager_->update_vector_version(1, 2);
    EXPECT_EQ(consistency_manager_->get_vector_version(1), 2);
    
    // Test conflict resolution
    IndexOperation op2(IndexOperation::UPDATE, 1, generate_random_vector(), 
                       std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch()).count(), 3);
    
    std::vector<IndexOperation> operations = {add_op, op2};
    auto resolved = consistency_manager_->resolve_conflict(operations);
    EXPECT_EQ(resolved.id, 1);
    EXPECT_EQ(resolved.version, 3); // Should pick the latest version
}

TEST_F(DANNTest, QueryRouterBasicOperations) {
    // Start node manager
    node_manager_->start();
    
    // Test query execution
    auto query_vector = generate_random_vector();
    QueryRequest request(query_vector, 5);
    
    auto response = query_router_->execute_query(request);
    EXPECT_TRUE(response.success);
    EXPECT_LE(response.results.size(), 5);
    
    // Test metrics
    auto metrics = query_router_->get_metrics();
    EXPECT_GT(metrics.total_queries, 0);
    EXPECT_GT(metrics.successful_queries, 0);
    
    // Reset metrics
    query_router_->reset_metrics();
    metrics = query_router_->get_metrics();
    EXPECT_EQ(metrics.total_queries, 0);
}

TEST_F(DANNTest, BulkLoaderBasicOperations) {
    // Test bulk loading
    BulkLoadRequest load_request(test_vectors_, test_ids_, 100);
    
    auto load_future = bulk_loader_->load_vectors(load_request);
    EXPECT_TRUE(load_future.get());
    
    // Verify vectors were loaded
    EXPECT_EQ(vector_index_->size(), test_ids_.size());
    
    // Test progress tracking
    auto active_loads = bulk_loader_->get_active_loads();
    EXPECT_GE(active_loads.size(), 0);
    
    // Test metrics
    auto metrics = bulk_loader_->get_metrics();
    EXPECT_GT(metrics.total_loads, 0);
    EXPECT_GT(metrics.successful_loads, 0);
    EXPECT_GT(metrics.total_vectors_loaded, 0);
}

TEST_F(DANNTest, BulkLoaderValidation) {
    // Test invalid vectors (empty)
    std::vector<float> empty_vectors;
    std::vector<int64_t> empty_ids;
    BulkLoadRequest empty_request(empty_vectors, empty_ids);
    
    auto empty_future = bulk_loader_->load_vectors(empty_request);
    EXPECT_FALSE(empty_future.get());
    
    // Test mismatched vectors and IDs
    std::vector<float> invalid_vectors(100, 1.0f); // 100 floats
    std::vector<int64_t> invalid_ids(10, 1); // 10 IDs (should be 100/128 = 0 or 100/128 = 0)
    
    BulkLoadRequest invalid_request(invalid_vectors, invalid_ids);
    auto invalid_future = bulk_loader_->load_vectors(invalid_request);
    EXPECT_FALSE(invalid_future.get());
}

TEST_F(DANNTest, IntegrationTest) {
    // Start all components
    node_manager_->start();
    consistency_manager_->start_anti_entropy();
    
    // Enable caching
    query_router_->enable_caching(true);
    
    // Load data
    BulkLoadRequest load_request(test_vectors_, test_ids_, 100);
    EXPECT_TRUE(bulk_loader_->load_vectors_sync(load_request));
    
    // Perform queries
    const int num_queries = 10;
    for (int i = 0; i < num_queries; ++i) {
        auto query_vector = generate_random_vector();
        QueryRequest request(query_vector, 5);
        
        auto response = query_router_->execute_query(request);
        EXPECT_TRUE(response.success);
        EXPECT_LE(response.results.size(), 5);
    }
    
    // Verify metrics
    auto query_metrics = query_router_->get_metrics();
    EXPECT_EQ(query_metrics.total_queries, num_queries);
    EXPECT_EQ(query_metrics.successful_queries, num_queries);
    
    auto load_metrics = bulk_loader_->get_metrics();
    EXPECT_EQ(load_metrics.total_loads, 1);
    EXPECT_EQ(load_metrics.successful_loads, 1);
    EXPECT_EQ(load_metrics.total_vectors_loaded, test_ids_.size());
}

TEST_F(DANNTest, PerformanceTest) {
    // Load larger dataset
    std::vector<float> large_vectors;
    std::vector<int64_t> large_ids;
    
    const size_t large_size = 10000;
    for (size_t i = 0; i < large_size; ++i) {
        auto vector = generate_random_vector();
        large_vectors.insert(large_vectors.end(), vector.begin(), vector.end());
        large_ids.push_back(static_cast<int64_t>(i));
    }
    
    // Measure bulk load performance
    auto load_start = std::chrono::high_resolution_clock::now();
    BulkLoadRequest large_load_request(large_vectors, large_ids, 1000);
    EXPECT_TRUE(bulk_loader_->load_vectors_sync(large_load_request));
    auto load_end = std::chrono::high_resolution_clock::now();
    
    auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start);
    std::cout << "Bulk loaded " << large_size << " vectors in " << load_time.count() << " ms\n";
    
    // Measure query performance
    const int num_performance_queries = 100;
    auto query_start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_performance_queries; ++i) {
        auto query_vector = generate_random_vector();
        QueryRequest request(query_vector, 10);
        auto response = query_router_->execute_query(request);
        EXPECT_TRUE(response.success);
    }
    
    auto query_end = std::chrono::high_resolution_clock::now();
    auto query_time = std::chrono::duration_cast<std::chrono::milliseconds>(query_end - query_start);
    
    std::cout << "Executed " << num_performance_queries << " queries in " 
              << query_time.count() << " ms\n";
    std::cout << "Average query time: " << (query_time.count() / num_performance_queries) << " ms\n";
    
    // Verify performance metrics
    auto metrics = query_router_->get_metrics();
    EXPECT_EQ(metrics.total_queries, num_performance_queries);
    EXPECT_LT(metrics.avg_response_time_ms, 100.0); // Should be under 100ms on average
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
