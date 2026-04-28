#include "dann/vector_index.h"
#include "dann/index.h"
#include "dann/index_persistence_manager.h"
#include "dann/distributed_index_ivf.h"
#include "dann/logger.h"
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
#include "dann/rpc_client.h"
#include "network/vector_search_service_impl.h"
#include "network/gateway_search_service_impl.h"
#endif

using namespace dann;
namespace fs = std::filesystem;

std::unique_ptr<IndexPersistenceManager> g_persistence_manager;
std::shared_ptr<Index> g_index;

void signal_handler(int signal) {
    LOG_INFO("Received signal " + std::to_string(signal) + ", shutting down...");
    if (g_persistence_manager) {
        g_persistence_manager->stop();
    }
    exit(0);
}

void print_usage() {
    LOG_INFO("DANN - Distributed Approximate Nearest Neighbors");
    LOG_INFO("Usage: dann_server [options]");
    LOG_INFO("Options:");
    LOG_INFO("  --role <role>          Node role: master, shard, standalone (default: standalone)");
    LOG_INFO("  --node-id <id>         Node identifier (default: node1)");
    LOG_INFO("  --address <addr>       Listen address (default: 0.0.0.0)");
    LOG_INFO("  --port <port>          Listen port (default: 8080)");
#ifdef HAVE_GRPC
    LOG_INFO("  --grpc-port <port>     gRPC server port (default: 50051)");
    LOG_INFO("  --shard-addresses <addrs>  Comma-separated shard addresses (e.g. localhost:50052,localhost:50053)");
#endif
    LOG_INFO("  --dimension <dim>      Vector dimension (default: 128)");
    LOG_INFO("  --index-type <type>    Index type: Flat, IVF, HNSW (default: IVF)");
    LOG_INFO("  --shards <shards>      Number of shards (default: 1)");
    LOG_INFO("  --shard-id <id>        Shard ID for shard role (default: 0)");
    LOG_INFO("  --index <index>        faiss index file");
    LOG_INFO("  --seed-nodes <nodes>   Comma-separated list of seed nodes");
    LOG_INFO("  --persistence-path <path>  Persistence storage path (default: ./data)");
    LOG_INFO("  --persistence-interval <sec>  Auto-save interval in seconds (default: 60)");
    LOG_INFO("  --no-persistence       Disable persistence");
    LOG_INFO("  --num-vectors <n>      Number of vectors for master to generate (default: 10000)");
    LOG_INFO("  --help                 Show this help message");
}

struct Config {
    std::string role = "standalone";
    std::string node_id = "node1";
    std::string address = "0.0.0.0";
    int port = 8080;
#ifdef HAVE_GRPC
    int grpc_port = 50051;
    std::vector<std::string> shard_addresses;
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
        } else if (arg == "--shard-addresses" && i + 1 < argc) {
            std::string addrs = argv[++i];
            size_t pos = 0;
            while ((pos = addrs.find(',')) != std::string::npos) {
                std::string addr = addrs.substr(0, pos);
                if (!addr.empty()) config.shard_addresses.push_back(addr);
                addrs.erase(0, pos + 1);
            }
            if (!addrs.empty()) config.shard_addresses.push_back(addrs);
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

void build_and_save_index(const Config& config) {
    std::vector<std::string> nodes;
    for (int i = 0; i < config.shard_count; ++i) {
        nodes.push_back("shard_" + std::to_string(i));
    }
    
    auto distributed_index = std::make_unique<DistributedIndexIVF>(
        "distributed_index", config.dimension, config.shard_count, nodes);
    
    LOG_INFO("Generating " + std::to_string(config.num_vectors) + " vectors...");
    std::vector<float> vectors;
    std::vector<int64_t> ids;
    auto gen_start = std::chrono::high_resolution_clock::now();
    generate_clustered_data(config.num_vectors, config.dimension, vectors, ids);
    auto gen_end = std::chrono::high_resolution_clock::now();
    auto gen_time = std::chrono::duration_cast<std::chrono::milliseconds>(gen_end - gen_start);
    LOG_INFO("Data generation completed in " + std::to_string(gen_time.count()) + " ms");
    
    LOG_INFO("Building index...");
    auto build_start = std::chrono::high_resolution_clock::now();
    distributed_index->add_vectors(vectors, ids);
    auto build_end = std::chrono::high_resolution_clock::now();
    auto build_time = std::chrono::duration_cast<std::chrono::milliseconds>(build_end - build_start);
    LOG_INFO("Index build completed in " + std::to_string(build_time.count()) + " ms");
    
    LOG_INFO("Saving index to " + config.persistence_path + "...");
    if (!distributed_index->save_index(config.persistence_path)) {
        LOG_ERROR("Failed to save index");
        return;
    }
    
    LOG_INFO("Index saved successfully!");
    for (int i = 0; i < config.shard_count; ++i) {
        LOG_INFO("  " + config.persistence_path + "/distributed_index.shards/shard_" + std::to_string(i) + ".posting");
    }
}

#ifdef HAVE_GRPC
struct ShardAddress {
    std::string host;
    int port;
};

static std::vector<ShardAddress> parse_shard_addresses(const std::vector<std::string>& addresses) {
    std::vector<ShardAddress> result;
    for (const auto& addr : addresses) {
        auto colon_pos = addr.rfind(':');
        if (colon_pos == std::string::npos) {
            LOG_ERROR("Invalid shard address format: " + addr + " (expected host:port)");
            continue;
        }
        ShardAddress sa;
        sa.host = addr.substr(0, colon_pos);
        sa.port = std::stoi(addr.substr(colon_pos + 1));
        result.push_back(sa);
    }
    return result;
}
#endif

void run_master_node(const Config& config) {
    LOG_INFO("=== Running as MASTER node (API Gateway) ===");
    LOG_INFO("  Node ID: " + config.node_id);
    LOG_INFO("  Dimension: " + std::to_string(config.dimension));
    LOG_INFO("  Shard count: " + std::to_string(config.shard_count));

#ifdef HAVE_GRPC
    LOG_INFO("  gRPC Port: " + std::to_string(config.grpc_port));

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (config.shard_addresses.empty()) {
        LOG_ERROR("Error: No shard addresses provided. Use --shard-addresses host:port,host:port");
        return;
    }

    auto shard_addrs = parse_shard_addresses(config.shard_addresses);
    if (shard_addrs.empty()) {
        LOG_ERROR("Error: No valid shard addresses");
        return;
    }

    build_and_save_index(config);

    auto gateway = std::make_unique<GatewaySearchServiceImpl>(
        static_cast<int>(shard_addrs.size()), config.dimension);

    for (size_t i = 0; i < shard_addrs.size(); ++i) {
        gateway->add_shard(static_cast<int>(i), shard_addrs[i].host, shard_addrs[i].port);
    }

    LOG_INFO("Connecting to shard nodes...");
    if (!gateway->connect_all_shards()) {
        LOG_WARN("Warning: Not all shards connected. Gateway will start but some requests may fail.");
    }

    auto rpc_server = std::make_shared<RPCServer>(config.address, config.grpc_port);
    rpc_server->register_gateway_service(std::move(gateway));
    rpc_server->set_max_threads(8);

    if (!rpc_server->start()) {
        LOG_ERROR("Failed to start gRPC server");
        return;
    }

    LOG_INFO("Master gateway started on port " + std::to_string(config.grpc_port));
    LOG_INFO("=== Master node ready ===");
    LOG_INFO("Press Enter to stop...");
    std::cin.get();

    LOG_INFO("Master node stopped.");
#else
    LOG_INFO("Note: gRPC support not compiled in. Master gateway requires gRPC.");
#endif
}

void run_shard_node(const Config& config) {
    LOG_INFO("=== Running as SHARD node ===");
    LOG_INFO("  Node ID: " + config.node_id);
    LOG_INFO("  Shard ID: " + std::to_string(config.shard_id));
    LOG_INFO("  Total Shards: " + std::to_string(config.shard_count));
    LOG_INFO("  Dimension: " + std::to_string(config.dimension));
#ifdef HAVE_GRPC
    LOG_INFO("  gRPC Port: " + std::to_string(config.grpc_port));
#endif
    LOG_INFO("  Persistence path: " + config.persistence_path + "\n");
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    auto shard_index = std::make_shared<DistributedIndexIVF>(
        "distributed_index", config.dimension, config.shard_count, config.shard_id, config.node_id);
    
    LOG_INFO("Loading shard " + std::to_string(config.shard_id) + " from " + config.persistence_path + "...");
    if (!shard_index->load_shard_only(config.persistence_path, config.shard_id)) {
        LOG_ERROR("Failed to load shard " + std::to_string(config.shard_id));
        return;
    }
    LOG_INFO("Shard loaded successfully!");
    LOG_INFO("Shard contains " + std::to_string(shard_index->size()) + " vectors");
    LOG_INFO("Dimension: " + std::to_string(config.dimension));

    g_index = std::make_shared<Index>("distributed_index", config.dimension, 1, "IVF");
    g_index->set_shard(0, shard_index);
    
#ifdef HAVE_GRPC
    auto rpc_server = std::make_shared<RPCServer>(config.address, config.grpc_port);
    auto search_service = std::make_unique<VectorSearchServiceImpl>(g_index);
    rpc_server->register_service(std::move(search_service));
    rpc_server->set_max_threads(8);
    
    if (!rpc_server->start()) {
        LOG_ERROR("Failed to start gRPC server");
        return;
    }
    LOG_INFO("gRPC server started on port " + std::to_string(config.grpc_port));
#else
    LOG_INFO("Note: gRPC support not compiled in");
#endif
    
    LOG_INFO("=== Shard node ready ===");
    LOG_INFO("Press Enter to stop...");
    std::cin.get();
    
#ifdef HAVE_GRPC
#endif
    LOG_INFO("Shard node stopped.");
}

void run_standalone_node(const Config& config) {
    LOG_INFO("=== Running in STANDALONE mode ===");
    LOG_INFO("  Node ID: " + config.node_id);
    LOG_INFO("  Address: " + config.address + ":" + std::to_string(config.port));
#ifdef HAVE_GRPC
    LOG_INFO("  gRPC Port: " + std::to_string(config.grpc_port));
#endif
    LOG_INFO("  Dimension: " + std::to_string(config.dimension));
    LOG_INFO("  Index Type: " + config.index_type);
    LOG_INFO("  Shards: " + std::to_string(config.shard_count));
    LOG_INFO("  Persistence: " + std::string(config.persistence_enabled ? "enabled" : "disabled"));
    if (config.persistence_enabled) {
        LOG_INFO("  Persistence Path: " + config.persistence_path);
        LOG_INFO("  Persistence Interval: " + std::to_string(config.persistence_interval) + "s");
    }
    LOG_INFO("");
    
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
            LOG_INFO("Persistence manager started");
        }
    }
    
#ifdef HAVE_GRPC
    auto rpc_server = std::make_shared<RPCServer>(config.address, config.grpc_port);
    auto search_service = std::make_unique<VectorSearchServiceImpl>(g_index);
    rpc_server->register_service(std::move(search_service));
    rpc_server->set_max_threads(8);
    
    if (!rpc_server->start()) {
        LOG_ERROR("Failed to start gRPC server");
        return;
    }
    
    LOG_INFO("gRPC server started on port " + std::to_string(config.grpc_port));
#else
    LOG_INFO("Running without gRPC support");
#endif
    
    LOG_INFO("=== Index Information ===");
    LOG_INFO("Index name: " + g_index->name());
    LOG_INFO("Index type: " + g_index->index_type());
    LOG_INFO("Index dimension: " + std::to_string(g_index->dimension()));
    LOG_INFO("Index size: " + std::to_string(g_index->size()) + " vectors");
    LOG_INFO("Shard count: " + std::to_string(g_index->shard_count()));
    
    LOG_INFO("Server running. Press Enter to stop...");
    std::cin.get();
    
    if (g_persistence_manager) {
        g_persistence_manager->stop();
    }
#ifdef HAVE_GRPC
#endif
    
    LOG_INFO("Server stopped.");
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
        LOG_ERROR("Error: " + std::string(e.what()));
        return 1;
    }
}
