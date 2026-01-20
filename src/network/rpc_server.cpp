#include "dann/rpc_server.h"
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <chrono>
#include <thread>

namespace dann {

RPCServer::RPCServer(const std::string& address, int port)
    : address_(address), port_(port), running_(false), 
      max_threads_(4), timeout_ms_(5000) {
    
    metrics_ = ServerMetrics{};
    metrics_.total_requests = 0;
    metrics_.successful_requests = 0;
    metrics_.failed_requests = 0;
    metrics_.avg_response_time_ms = 0.0;
    metrics_.active_connections = 0;
}

RPCServer::~RPCServer() {
    stop();
}

bool RPCServer::start() {
    if (running_.load()) {
        return true;
    }
    
    try {
        setup_grpc_server();
        start_worker_threads();
        
        running_ = true;
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool RPCServer::stop() {
    if (!running_.load()) {
        return true;
    }
    
    running_ = false;
    
    if (server_) {
        server_->Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(5));
    }
    
    stop_worker_threads();
    
    return true;
}

bool RPCServer::is_running() const {
    return running_.load();
}

void RPCServer::register_service(std::shared_ptr<VectorSearchService> service) {
    search_service_ = service;
}

void RPCServer::set_max_threads(int max_threads) {
    max_threads_ = std::max(1, max_threads);
}

void RPCServer::set_timeout_ms(int timeout_ms) {
    timeout_ms_ = std::max(100, timeout_ms);
}

RPCServer::ServerMetrics RPCServer::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}

void RPCServer::reset_metrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_ = ServerMetrics{};
    metrics_.total_requests = 0;
    metrics_.successful_requests = 0;
    metrics_.failed_requests = 0;
    metrics_.avg_response_time_ms = 0.0;
    metrics_.active_connections = 0;
}

void RPCServer::setup_grpc_server() {
    grpc::ServerBuilder builder;
    
    // Listen on the given address
    std::string server_address = address_ + ":" + std::to_string(port_);
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    
    // Set max message size
    builder.SetMaxReceiveMessageSize(100 * 1024 * 1024); // 100MB
    builder.SetMaxSendMessageSize(100 * 1024 * 1024);    // 100MB
    
    // Set max threads
    builder.SetSyncServerOption(grpc::ServerBuilder::SyncServerOption::MAX_POLLERS, max_threads_);
    
    // Build and start server
    server_ = builder.BuildAndStart();
    if (!server_) {
        throw std::runtime_error("Failed to start gRPC server");
    }
    
    std::cout << "gRPC server listening on " << server_address << std::endl;
}

void RPCServer::handle_request(const std::string& method, const std::string& request_data) {
    auto start_time = std::chrono::high_resolution_clock::now();
    bool success = false;
    
    try {
        if (!search_service_) {
            throw std::runtime_error("No service registered");
        }
        
        // Handle different request types based on method
        if (method == "Search") {
            // Deserialize QueryRequest and call service
            // This is simplified - in real implementation would use protobuf
            // QueryRequest request = deserialize_query_request(request_data);
            // QueryResponse response = search_service_->Search(request);
            success = true;
        } else if (method == "AddVectors") {
            // BulkLoadRequest request = deserialize_bulk_load_request(request_data);
            // bool result = search_service_->AddVectors(request);
            success = true;
        }
        // ... other methods
        
    } catch (const std::exception& e) {
        success = false;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    update_metrics(success, response_time.count());
}

void RPCServer::update_metrics(bool success, double response_time) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    metrics_.total_requests++;
    
    if (success) {
        metrics_.successful_requests++;
    } else {
        metrics_.failed_requests++;
    }
    
    // Update average response time
    double total_time = metrics_.avg_response_time_ms * (metrics_.total_requests - 1);
    metrics_.avg_response_time_ms = (total_time + response_time) / metrics_.total_requests;
}

void RPCServer::worker_loop() {
    while (running_.load()) {
        // Process incoming requests
        // This is simplified - in real implementation would use gRPC's async API
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void RPCServer::start_worker_threads() {
    for (int i = 0; i < max_threads_; ++i) {
        worker_threads_.emplace_back(&RPCServer::worker_loop, this);
    }
}

void RPCServer::stop_worker_threads() {
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
}

} // namespace dann
