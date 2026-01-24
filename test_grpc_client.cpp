#include <grpc++/grpc++.h>
#include "dann/vector_service.grpc.pb.h"
#include <iostream>
#include <vector>
#include <random>
#include <chrono>

using namespace grpc;
using namespace dann;

class VectorSearchClient {
public:
    VectorSearchClient(std::shared_ptr<Channel> channel)
        : stub_(VectorSearchService::NewStub(channel)) {}

    bool AddVectors(const std::vector<std::vector<float>>& vectors, 
                   const std::vector<int64_t>& ids) {
        AddVectorsRequest request;
        request.set_batch_size(1000);
        request.set_overwrite_existing(false);
        
        for (size_t i = 0; i < vectors.size(); ++i) {
            auto* vector = request.add_vectors();
            vector->set_id(ids[i]);
            for (float value : vectors[i]) {
                vector->add_data(value);
            }
        }
        
        AddVectorsResponse response;
        ClientContext context;
        
        auto start = std::chrono::high_resolution_clock::now();
        Status status = stub_->AddVectors(&context, request, &response);
        auto end = std::chrono::high_resolution_clock::now();
        
        if (status.ok()) {
            std::cout << "AddVectors succeeded: " << response.added_count() 
                      << " vectors, time_ms: " << response.load_time_ms() << std::endl;
            return true;
        } else {
            std::cout << "AddVectors failed: " << status.error_message() << std::endl;
            return false;
        }
    }

    bool Search(const std::vector<float>& query_vector, int k) {
        SearchRequest request;
        for (float value : query_vector) {
            request.add_query_vector(value);
        }
        request.set_k(k);
        request.set_consistency_level("eventual");
        request.set_timeout_ms(5000);
        
        SearchResponse response;
        ClientContext context;
        
        auto start = std::chrono::high_resolution_clock::now();
        Status status = stub_->Search(&context, request, &response);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto query_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (status.ok()) {
            std::cout << "Search succeeded: " << response.results_size() << " results" << std::endl;
            std::cout << "Query time: " << response.query_time_ms() << " ms (client: " << query_time.count() << " ms)" << std::endl;
            
            for (int i = 0; i < response.results_size() && i < 5; ++i) {
                const auto& result = response.results(i);
                std::cout << "  Result " << (i+1) << ": ID=" << result.id() 
                          << ", Distance=" << result.distance() << std::endl;
            }
            return true;
        } else {
            std::cout << "Search failed: " << status.error_message() << std::endl;
            return false;
        }
    }

    bool GetStats() {
        StatsRequest request;
        StatsResponse response;
        ClientContext context;
        
        Status status = stub_->GetStats(&context, request, &response);
        
        if (status.ok()) {
            std::cout << "Stats:" << std::endl;
            std::cout << "  Total vectors: " << response.total_vectors() << std::endl;
            std::cout << "  Index type: " << response.index_type() << std::endl;
            std::cout << "  Dimension: " << response.dimension() << std::endl;
            std::cout << "  Total queries: " << response.total_queries() << std::endl;
            std::cout << "  Avg query time: " << response.avg_query_time_ms() << " ms" << std::endl;
            return true;
        } else {
            std::cout << "GetStats failed: " << status.error_message() << std::endl;
            return false;
        }
    }

    bool HealthCheck() {
        HealthCheckRequest request;
        HealthCheckResponse response;
        ClientContext context;
        
        Status status = stub_->HealthCheck(&context, request, &response);
        
        if (status.ok()) {
            std::cout << "Health Check:" << std::endl;
            std::cout << "  Healthy: " << (response.healthy() ? "Yes" : "No") << std::endl;
            std::cout << "  Status: " << response.status() << std::endl;
            std::cout << "  Version: " << response.version() << std::endl;
            std::cout << "  Uptime: " << response.uptime_seconds() << " seconds" << std::endl;
            return true;
        } else {
            std::cout << "HealthCheck failed: " << status.error_message() << std::endl;
            return false;
        }
    }

private:
    std::unique_ptr<VectorSearchService::Stub> stub_;
};

std::vector<float> generate_random_vector(int dimension) {
    std::vector<float> vector(dimension);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0f, 1.0f);
    
    for (int i = 0; i < dimension; ++i) {
        vector[i] = dist(gen);
    }
    
    return vector;
}

int main(int argc, char** argv) {
    std::string server_address = "localhost:50051";
    if (argc > 1) {
        server_address = argv[1];
    }
    
    // Create channel and client
    auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
    VectorSearchClient client(channel);
    
    std::cout << "Connecting to DANN server at " << server_address << std::endl;
    
    // Health check
    std::cout << "\n=== Health Check ===" << std::endl;
    client.HealthCheck();
    
    // Get initial stats
    std::cout << "\n=== Initial Stats ===" << std::endl;
    client.GetStats();
    
    // Add some test vectors
    std::cout << "\n=== Adding Test Vectors ===" << std::endl;
    const int dimension = 128;
    const int num_vectors = 1000;
    
    std::vector<std::vector<float>> vectors;
    std::vector<int64_t> ids;
    
    for (int i = 0; i < num_vectors; ++i) {
        vectors.push_back(generate_random_vector(dimension));
        ids.push_back(i);
    }
    
    if (!client.AddVectors(vectors, ids)) {
        std::cerr << "Failed to add vectors" << std::endl;
        return 1;
    }
    
    // Get stats after adding vectors
    std::cout << "\n=== Stats After Adding Vectors ===" << std::endl;
    client.GetStats();
    
    // Perform some test searches
    std::cout << "\n=== Test Searches ===" << std::endl;
    for (int i = 0; i < 5; ++i) {
        std::cout << "\nSearch " << (i+1) << ":" << std::endl;
        auto query_vector = generate_random_vector(dimension);
        client.Search(query_vector, 10);
    }
    
    // Final stats
    std::cout << "\n=== Final Stats ===" << std::endl;
    client.GetStats();
    
    return 0;
}
