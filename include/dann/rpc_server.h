#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include "dann/types.h"
#include "vector_service.pb.h"
#include "vector_service.grpc.pb.h"

namespace dann {

// Forward declaration
class VectorSearchServiceImpl;

class RPCServer {
public:
    RPCServer(const std::string& address, int port);
    ~RPCServer();
    
    // Server lifecycle
    bool start();
    bool stop();
    bool is_running() const;
    
    // Service registration
    void register_service(std::unique_ptr<VectorSearchServiceImpl> service);
    
    // Configuration
    void set_max_threads(int max_threads);
    void set_timeout_ms(int timeout_ms);
    
    // Metrics
    struct ServerMetrics {
        uint64_t total_requests;
        uint64_t successful_requests;
        uint64_t failed_requests;
        double avg_response_time_ms;
        uint64_t active_connections;
    };
    
    ServerMetrics get_metrics() const;
    void reset_metrics();
    
private:
    std::string address_;
    int port_;
    std::atomic<bool> running_;
    int max_threads_;
    int timeout_ms_;
    
    std::unique_ptr<grpc::Server> server_;
    std::unique_ptr<VectorSearchServiceImpl> search_service_;
    
    mutable std::mutex metrics_mutex_;
    ServerMetrics metrics_;
    
    std::vector<std::thread> worker_threads_;
    
    // Server implementation
    void setup_grpc_server();
    void handle_request(const std::string& method, const std::string& request_data);
    void update_metrics(bool success, double response_time);
    
    // Worker threads
    void worker_loop();
    void start_worker_threads();
    void stop_worker_threads();
};

} // namespace dann
