#include "dann/vector_index.h"
#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <random>

#ifdef HAVE_GRPC
#include "dann/rpc_server.h"
#include "network/vector_search_service_impl.h"
#endif

using namespace dann;

void print_usage() {
    std::cout << "DANN - Distributed Approximate Nearest Neighbors\n";
    std::cout << "Usage: dann_server [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --node-id <id>        Node identifier (default: node1)\n";
    std::cout << "  --address <addr>       Listen address (default: 0.0.0.0)\n";
    std::cout << "  --port <port>         Listen port (default: 8080)\n";
#ifdef HAVE_GRPC
    std::cout << "  --grpc-port <port>    gRPC server port (default: 50051)\n";
#endif
    std::cout << "  --dimension <dim>     Vector dimension (default: 128)\n";
    std::cout << "  --index-type <type>   Index type: Flat, IVF, HNSW (default: IVF)\n";
    std::cout << "  --seed-nodes <nodes>  Comma-separated list of seed nodes\n";
    std::cout << "  --help                Show this help message\n";
}

struct Config {
    std::string node_id = "node1";
    std::string address = "0.0.0.0";
    int port = 8080;
#ifdef HAVE_GRPC
    int grpc_port = 50051;
#endif
    int dimension = 128;
    std::string index_type = "IVF";
    std::vector<std::string> seed_nodes;
};

Config parse_arguments(int argc, char* argv[]) {
    Config config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            print_usage();
            exit(0);
        } else if (arg == "--node-id" && i + 1 < argc) {
            config.node_id = argv[++i];
        } else if (arg == "--address" && i + 1 < argc) {
            config.address = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = std::stoi(argv[++i]);
#ifdef HAVE_GRPC
        } else if (arg == "--grpc-port" && i + 1 < argc) {
            config.grpc_port = std::stoi(argv[++i]);
#endif
        } else if (arg == "--dimension" && i + 1 < argc) {
            config.dimension = std::stoi(argv[++i]);
        } else if (arg == "--index-type" && i + 1 < argc) {
            config.index_type = argv[++i];
        } else if (arg == "--seed-nodes" && i + 1 < argc) {
            std::string seeds = argv[++i];
            size_t pos = 0;
            while ((pos = seeds.find(',')) != std::string::npos) {
                config.seed_nodes.push_back(seeds.substr(0, pos));
                seeds.erase(0, pos + 1);
            }
            if (!seeds.empty()) {
                config.seed_nodes.push_back(seeds);
            }
        }
    }
    
    return config;
}

std::vector<float> generate_random_vector(int dimension, std::mt19937& gen) {
    std::vector<float> vector(dimension);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    
    for (int i = 0; i < dimension; ++i) {
        vector[i] = dist(gen);
    }
    
    return vector;
}

void run_demo(const Config& config) {
    std::cout << "Starting DANN demo with configuration:\n";
    std::cout << "  Node ID: " << config.node_id << "\n";
    std::cout << "  Address: " << config.address << ":" << config.port << "\n";
#ifdef HAVE_GRPC
    std::cout << "  gRPC Port: " << config.grpc_port << "\n";
#endif
    std::cout << "  Dimension: " << config.dimension << "\n";
    std::cout << "  Index Type: " << config.index_type << "\n";
    std::cout << "\n";
    
    // Create components
    auto vector_index = std::make_shared<VectorIndex>(config.dimension, config.index_type);
    // auto node_manager = std::make_shared<NodeManager>(config.node_id, config.address, config.port);
    // auto consistency_manager = std::make_shared<ConsistencyManager>(config.node_id);
    // auto query_router = std::make_shared<QueryRouter>(node_manager);
    // auto bulk_loader = std::make_shared<BulkLoader>(vector_index, consistency_manager);
    
#ifdef HAVE_GRPC
    // Create and start gRPC server
    auto rpc_server = std::make_shared<RPCServer>(config.address, config.grpc_port);
    auto search_service = std::make_unique<VectorSearchServiceImpl>(vector_index);
    rpc_server->register_service(std::move(search_service));
    rpc_server->set_max_threads(8);
    
    if (!rpc_server->start()) {
        std::cerr << "Failed to start gRPC server\n";
        return;
    }
    
    std::cout << "gRPC server started on port " << config.grpc_port << "\n";
#else
    std::cout << "Running without gRPC support\n";
#endif
    
    // Start node manager
    // if (!node_manager->start()) {
    //     std::cerr << "Failed to start node manager\n";
    //     return;
    // }
    //
    // // Join cluster if seed nodes provided
    // if (!config.seed_nodes.empty()) {
    //     std::cout << "Joining cluster with seed nodes: ";
    //     for (const auto& seed : config.seed_nodes) {
    //         std::cout << seed << " ";
    //     }
    //     std::cout << "\n";
    //
    //     node_manager->join_cluster(config.seed_nodes);
    // }
    //
    // // Start consistency manager
    // consistency_manager->start_anti_entropy();
    
    // Enable query caching
    // query_router->enable_caching(true);
    // query_router->set_load_balance_strategy("round_robin");
    
    // Generate sample data
    std::cout << "Generating sample data...\n";
    std::random_device rd;
    std::mt19937 gen(rd());
    
    const size_t num_vectors = 10000;
    std::vector<float> vectors;
    std::vector<int64_t> ids;
    
    for (size_t i = 0; i < num_vectors; ++i) {
        auto vector = generate_random_vector(config.dimension, gen);
        vectors.insert(vectors.end(), vector.begin(), vector.end());
        ids.push_back(static_cast<int64_t>(i));
    }
    
    // Bulk load vectors
    // std::cout << "Bulk loading " << num_vectors << " vectors...\n";
    // // BulkLoadRequest load_request(vectors, ids, 1000);
    // //
    // // auto load_start = std::chrono::high_resolution_clock::now();
    // // auto load_future = bulk_loader->load_vectors(load_request);
    
    // Monitor load progress
    // std::string load_id = bulk_loader->get_active_loads()[0];
    // while (true) {
    //     auto progress = bulk_loader->get_progress(load_id);
    //     std::cout << "  Progress: " << progress.progress_percentage << "% "
    //               << "(" << progress.processed_vectors << "/" << progress.total_vectors << ") "
    //               << "Status: " << progress.status << "\r";
    //
    //     if (progress.status == "completed" || progress.status == "completed_with_errors" ||
    //         progress.status == "failed" || progress.status == "cancelled") {
    //         break;
    //     }
    //
    //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // }
    //
    // auto load_result = load_future.get();
    // auto load_end = std::chrono::high_resolution_clock::now();
    // auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start);
    //
    // std::cout << "\nBulk load " << (load_result ? "succeeded" : "failed")
    //           << " in " << load_time.count() << " ms\n";
    
    // Perform sample queries
    std::cout << "\nPerforming sample queries...\n";
    const int num_queries = 10;
    
    // for (int i = 0; i < num_queries; ++i) {
    //     auto query_vector = generate_random_vector(config.dimension, gen);
    //     QueryRequest query(query_vector, 10);
    //
    //     auto query_start = std::chrono::high_resolution_clock::now();
    //     auto response = query_router->execute_query(query);
    //     auto query_end = std::chrono::high_resolution_clock::now();
    //     auto query_time = std::chrono::duration_cast<std::chrono::milliseconds>(query_end - query_start);
    //
    //     std::cout << "Query " << (i + 1) << ": "
    //               << (response.success ? "Success" : "Failed")
    //               << ", Time: " << query_time.count() << " ms"
    //               << ", Results: " << response.results.size() << "\n";
    // }
    //
    // // Show metrics
    // std::cout << "\n=== Performance Metrics ===\n";
    //
    // auto query_metrics = query_router->get_metrics();
    // std::cout << "Query Metrics:\n";
    // std::cout << "  Total queries: " << query_metrics.total_queries << "\n";
    // std::cout << "  Successful queries: " << query_metrics.successful_queries << "\n";
    // std::cout << "  Failed queries: " << query_metrics.failed_queries << "\n";
    // std::cout << "  Average response time: " << query_metrics.avg_response_time_ms << " ms\n";
    //
    // auto load_metrics = bulk_loader->get_metrics();
    // std::cout << "\nBulk Load Metrics:\n";
    // std::cout << "  Total loads: " << load_metrics.total_loads << "\n";
    // std::cout << "  Successful loads: " << load_metrics.successful_loads << "\n";
    // std::cout << "  Failed loads: " << load_metrics.failed_loads << "\n";
    // std::cout << "  Average load time: " << load_metrics.avg_load_time_ms << " ms\n";
    // std::cout << "  Total vectors loaded: " << load_metrics.total_vectors_loaded << "\n";
    // std::cout << "  Average vectors/second: " << load_metrics.avg_vectors_per_second << "\n";
    //
    // // Show cluster information
    // std::cout << "\n=== Cluster Information ===\n";
    // auto cluster_nodes = node_manager->get_cluster_nodes();
    // std::cout << "Cluster nodes (" << cluster_nodes.size() << "):\n";
    // for (const auto& node : cluster_nodes) {
    //     std::cout << "  " << node.node_id << ": " << node.address << ":" << node.port
    //               << " (" << (node.is_active ? "active" : "inactive") << ")\n";
    // }
    //
    // auto assigned_shards = node_manager->get_assigned_shards();
    // std::cout << "Assigned shards: ";
    // for (int shard : assigned_shards) {
    //     std::cout << shard << " ";
    // }
    // std::cout << "\n";
    
    // Index information
    std::cout << "\n=== Index Information ===\n";
    std::cout << "Index type: " << vector_index->index_type() << "\n";
    std::cout << "Index dimension: " << vector_index->dimension() << "\n";
    std::cout << "Index size: " << vector_index->size() << " vectors\n";
    std::cout << "Index version: " << vector_index->get_version() << "\n";
    
    // Keep server running
    std::cout << "\nServer running. Press Enter to stop...\n";
    std::cin.get();
    
    // Cleanup
    // consistency_manager->stop_anti_entropy();
    // node_manager->stop();
#ifdef HAVE_GRPC
    // rpc_server->stop();
#endif
    
    std::cout << "Server stopped.\n";
}

int main(int argc, char* argv[]) {
    try {
        Config config = parse_arguments(argc, argv);
        run_demo(config);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
