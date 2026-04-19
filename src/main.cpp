#include "dann/vector_index.h"
#include "dann/index.h"
#include "dann/index_persistence_manager.h"
#include "dann/distributed_index_ivf.h"
#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <random>
#include <filesystem>
#include <cstdlib>
#include <signal.h>

#ifdef HAVE_GRPC
#include "dann/rpc_server.h"
#include "network/vector_search_service_impl.h"
#endif

using namespace dann;
namespace fs = std::filesystem;

std::unique_ptr<IndexPersistenceManager> g_persistence_manager;
std::shared_ptr<Index> g_index;

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down...\n";
    if (g_persistence_manager) {
        g_persistence_manager->stop();
    }
    exit(0);
}

void print_usage() {
    std::cout << "DANN - Distributed Approximate Nearest Neighbors\n";
    std::cout << "Usage: dann_server [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --role <role>          Node role: master, shard, standalone (default: standalone)\n";
    std::cout << "  --node-id <id>         Node identifier (default: node1)\n";
    std::cout << "  --address <addr>       Listen address (default: 0.0.0.0)\n";
    std::cout << "  --port <port>          Listen port (default: 8080)\n";
#ifdef HAVE_GRPC
    std::cout << "  --grpc-port <port>     gRPC server port (default: 50051)\n";
#endif
    std::cout << "  --dimension <dim>      Vector dimension (default: 128)\n";
    std::cout << "  --index-type <type>    Index type: Flat, IVF, HNSW (default: IVF)\n";
    std::cout << "  --shards <shards>      Number of shards (default: 1)\n";
    std::cout << "  --shard-id <id>        Shard ID for shard role (default: 0)\n";
    std::cout << "  --index <index>        faiss index file\n";
    std::cout << "  --seed-nodes <nodes>   Comma-separated list of seed nodes\n";
    std::cout << "  --persistence-path <path>  Persistence storage path (default: ./data)\n";
    std::cout << "  --persistence-interval <sec>  Auto-save interval in seconds (default: 60)\n";
    std::cout << "  --no-persistence       Disable persistence\n";
    std::cout << "  --num-vectors <n>      Number of vectors for master to generate (default: 10000)\n";
    std::cout << "  --help                 Show this help message\n";
}

struct Config {
    std::string role = "standalone";
    std::string node_id = "node1";
    std::string address = "0.0.0.0";
    int port = 8080;
#ifdef HAVE_GRPC
    int grpc_port = 50051;
#endif
    int dimension = 128;
    std::string index_type = "IVF";
    std::string index_path = "";
    int shard_count = 1;
    int shard_id = 0;
    std::vector<std::string> seed_nodes;
    int hnsw_m = 16;
    int hnsw_ef_construction = 100;
    bool persistence_enabled = true;
    int persistence_interval = 60;
    std::string persistence_path = "./data";
    int num_vectors = 10000;
};

std::string to_absolute_path(const std::string& path) {
    if (path.empty()) {
        return path;
    }
    
    fs::path p(path);
    if (p.is_absolute()) {
        return path;
    }
    
    // Get current working directory
    fs::path current_dir = fs::current_path();
    fs::path absolute_path = current_dir / p;
    
    return absolute_path.lexically_normal().string();
}

Config parse_arguments(int argc, char* argv[]) {
    Config config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            print_usage();
            exit(0);
        } else if (arg == "--role" && i + 1 < argc) {
            config.role = argv[++i];
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
        } else if (arg == "--shards" && i + 1 < argc) {
            config.shard_count = std::stoi(argv[++i]);
        } else if (arg == "--shard-id" && i + 1 < argc) {
            config.shard_id = std::stoi(argv[++i]);
        } else if (arg == "--index") {
            config.index_path = to_absolute_path(argv[++i]);
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
        } else if (arg == "--persistence-path" && i + 1 < argc) {
            config.persistence_path = argv[++i];
        } else if (arg == "--persistence-interval" && i + 1 < argc) {
            config.persistence_interval = std::stoi(argv[++i]);
        } else if (arg == "--no-persistence") {
            config.persistence_enabled = false;
        } else if (arg == "--num-vectors" && i + 1 < argc) {
            config.num_vectors = std::stoi(argv[++i]);
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

void generate_clustered_data(int n, int d, std::vector<float>& vectors, std::vector<int64_t>& ids) {
    vectors.clear();
    ids.clear();
    vectors.reserve(static_cast<size_t>(n) * d);
    ids.reserve(static_cast<size_t>(n));
    
    std::mt19937 gen(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    
    for (int i = 0; i < n; ++i) {
        const float base = static_cast<float>(i / 100) * 10.0f;
        for (int j = 0; j < d; ++j) {
            vectors.push_back(base + static_cast<float>(j) * 0.01f + dist(gen) * 0.1f);
        }
        ids.push_back(static_cast<int64_t>(i));
    }
}

void run_master_node(const Config& config) {
    std::cout << "=== Running as MASTER node ===\n";
    std::cout << "  Node ID: " << config.node_id << "\n";
    std::cout << "  Dimension: " << config.dimension << "\n";
    std::cout << "  Shard count: " << config.shard_count << "\n";
    std::cout << "  Num vectors: " << config.num_vectors << "\n";
    std::cout << "  Persistence path: " << config.persistence_path << "\n\n";
    
    std::vector<std::string> nodes;
    for (int i = 0; i < config.shard_count; ++i) {
        nodes.push_back("shard_" + std::to_string(i));
    }
    
    auto distributed_index = std::make_unique<DistributedIndexIVF>(
        "distributed_index", config.dimension, config.shard_count, nodes);
    
    std::cout << "Generating " << config.num_vectors << " vectors...\n";
    std::vector<float> vectors;
    std::vector<int64_t> ids;
    auto gen_start = std::chrono::high_resolution_clock::now();
    generate_clustered_data(config.num_vectors, config.dimension, vectors, ids);
    auto gen_end = std::chrono::high_resolution_clock::now();
    auto gen_time = std::chrono::duration_cast<std::chrono::milliseconds>(gen_end - gen_start);
    std::cout << "Data generation completed in " << gen_time.count() << " ms\n";
    
    std::cout << "Building index...\n";
    auto build_start = std::chrono::high_resolution_clock::now();
    distributed_index->add_vectors(vectors, ids);
    auto build_end = std::chrono::high_resolution_clock::now();
    auto build_time = std::chrono::duration_cast<std::chrono::milliseconds>(build_end - build_start);
    std::cout << "Index build completed in " << build_time.count() << " ms\n";
    
    std::cout << "Saving index to " << config.persistence_path << "...\n";
    if (!distributed_index->save_index(config.persistence_path)) {
        std::cerr << "Failed to save index\n";
        return;
    }
    
    std::cout << "Index saved successfully!\n";
    std::cout << "\n=== Master node completed ===\n";
    std::cout << "Shard files written to:\n";
    for (int i = 0; i < config.shard_count; ++i) {
        std::cout << "  " << config.persistence_path << "/distributed_index.shards/shard_" << i << ".posting\n";
    }
}

void run_shard_node(const Config& config) {
    std::cout << "=== Running as SHARD node ===\n";
    std::cout << "  Node ID: " << config.node_id << "\n";
    std::cout << "  Shard ID: " << config.shard_id << "\n";
    std::cout << "  Total Shards: " << config.shard_count << "\n";
    std::cout << "  Dimension: " << config.dimension << "\n";
#ifdef HAVE_GRPC
    std::cout << "  gRPC Port: " << config.grpc_port << "\n";
#endif
    std::cout << "  Persistence path: " << config.persistence_path << "\n\n";
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    auto shard_index = std::make_shared<DistributedIndexIVF>(
        "distributed_index", config.dimension, config.shard_count, config.shard_id, config.node_id);
    
    std::cout << "Loading shard " << config.shard_id << " from " << config.persistence_path << "...\n";
    if (!shard_index->load_shard_only(config.persistence_path, config.shard_id)) {
        std::cerr << "Failed to load shard " << config.shard_id << "\n";
        return;
    }
    std::cout << "Shard loaded successfully!\n";
    std::cout << "Shard contains " << shard_index->size() << " vectors\n";
    
    g_index = std::make_shared<Index>("distributed_index", config.dimension, 1, "IVF");
    g_index->set_shard(0, shard_index);
    
#ifdef HAVE_GRPC
    auto rpc_server = std::make_shared<RPCServer>(config.address, config.grpc_port);
    auto search_service = std::make_unique<VectorSearchServiceImpl>(g_index);
    rpc_server->register_service(std::move(search_service));
    rpc_server->set_max_threads(8);
    
    if (!rpc_server->start()) {
        std::cerr << "Failed to start gRPC server\n";
        return;
    }
    std::cout << "gRPC server started on port " << config.grpc_port << "\n";
#else
    std::cout << "Note: gRPC support not compiled in\n";
#endif
    
    std::cout << "\n=== Shard node ready ===\n";
    std::cout << "Press Enter to stop...\n";
    std::cin.get();
    
#ifdef HAVE_GRPC
#endif
    std::cout << "Shard node stopped.\n";
}

void run_standalone_node(const Config& config) {
    std::cout << "=== Running in STANDALONE mode ===\n";
    std::cout << "  Node ID: " << config.node_id << "\n";
    std::cout << "  Address: " << config.address << ":" << config.port << "\n";
#ifdef HAVE_GRPC
    std::cout << "  gRPC Port: " << config.grpc_port << "\n";
#endif
    std::cout << "  Dimension: " << config.dimension << "\n";
    std::cout << "  Index Type: " << config.index_type << "\n";
    std::cout << "  Shards: " << config.shard_count << "\n";
    std::cout << "  Persistence: " << (config.persistence_enabled ? "enabled" : "disabled") << "\n";
    if (config.persistence_enabled) {
        std::cout << "  Persistence Path: " << config.persistence_path << "\n";
        std::cout << "  Persistence Interval: " << config.persistence_interval << "s\n";
    }
    std::cout << "\n";
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    g_index = std::make_shared<Index>("default", 
        config.dimension, config.shard_count, config.index_type, 
        config.hnsw_m, config.hnsw_ef_construction, config.seed_nodes);

    if (!config.index_path.empty()) {
        if (g_index->shard_count() == 1) {
            g_index->shard(0)->load_index(config.index_path);
        }
    }
    
    if (config.persistence_enabled && config.index_type == "IVF") {
        auto ivf_shard = std::dynamic_pointer_cast<DistributedIndexIVF>(g_index->shard(0));
        if (ivf_shard) {
            PersistenceConfig persist_config(
                config.persistence_enabled,
                config.persistence_interval,
                config.persistence_path
            );
            g_persistence_manager = std::make_unique<IndexPersistenceManager>(
                ivf_shard.get(), persist_config
            );
            g_persistence_manager->start();
            std::cout << "Persistence manager started\n";
        }
    }
    
#ifdef HAVE_GRPC
    auto rpc_server = std::make_shared<RPCServer>(config.address, config.grpc_port);
    auto search_service = std::make_unique<VectorSearchServiceImpl>(g_index);
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
    
    std::cout << "\n=== Index Information ===\n";
    std::cout << "Index name: " << g_index->name() << "\n";
    std::cout << "Index type: " << g_index->index_type() << "\n";
    std::cout << "Index dimension: " << g_index->dimension() << "\n";
    std::cout << "Index size: " << g_index->size() << " vectors\n";
    std::cout << "Shard count: " << g_index->shard_count() << "\n";
    
    std::cout << "\nServer running. Press Enter to stop...\n";
    std::cin.get();
    
    if (g_persistence_manager) {
        g_persistence_manager->stop();
    }
#ifdef HAVE_GRPC
#endif
    
    std::cout << "Server stopped.\n";
}

void run_demo(const Config& config) {
    if (config.role == "master") {
        run_master_node(config);
    } else if (config.role == "shard") {
        run_shard_node(config);
    } else {
        run_standalone_node(config);
    }
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
