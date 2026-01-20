#pragma once

#include <grpc++/grpc++.h>
#include <memory>
#include <string>
#include <future>
#include <atomic>
#include <mutex>
#include "dann/types.h"

namespace dann {

class RPCClient {
public:
    RPCClient(const std::string& address, int port);
    ~RPCClient();
    
    // Connection management
    bool connect();
    bool disconnect();
    bool is_connected() const;
    
    // RPC operations
    std::future<QueryResponse> search_async(const QueryRequest& request);
    QueryResponse search_sync(const QueryRequest& request);
    
    std::future<bool> add_vectors_async(const BulkLoadRequest& request);
    bool add_vectors_sync(const BulkLoadRequest& request);
    
    std::future<bool> remove_vector_async(int64_t id);
    bool remove_vector_sync(int64_t id);
    
    std::future<bool> update_vector_async(int64_t id, const std::vector<float>& vector);
    bool update_vector_sync(int64_t id, const std::vector<float>& vector);
    
    // Configuration
    void set_timeout_ms(int timeout_ms);
    void set_max_retries(int max_retries);
    void enable_compression(bool enable);
    
    // Health check
    bool health_check();
    std::string get_server_info();
    
    // Metrics
    struct ClientMetrics {
        uint64_t total_requests;
        uint64_t successful_requests;
        uint64_t failed_requests;
        uint64_t retries;
        double avg_response_time_ms;
        uint64_t bytes_sent;
        uint64_t bytes_received;
    };
    
    ClientMetrics get_metrics() const;
    void reset_metrics();
    
private:
    std::string address_;
    int port_;
    std::atomic<bool> connected_;
    int timeout_ms_;
    int max_retries_;
    bool compression_enabled_;
    
    std::unique_ptr<grpc::Channel> channel_;
    std::unique_ptr<grpc::ClientContext> context_;
    
    mutable std::mutex metrics_mutex_;
    ClientMetrics metrics_;
    
    // Connection management
    bool create_channel();
    void close_channel();
    
    // RPC implementation
    template<typename Request, typename Response>
    std::future<Response> send_request_async(const std::string& method, const Request& request);
    
    template<typename Request, typename Response>
    Response send_request_sync(const std::string& method, const Request& request);
    
    // Retry logic
    template<typename Request, typename Response>
    Response send_with_retry(const std::string& method, const Request& request);
    
    // Metrics
    void update_metrics(bool success, double response_time, size_t bytes_sent, size_t bytes_received);
    
    // Serialization helpers
    std::string serialize_request(const QueryRequest& request);
    std::string serialize_request(const BulkLoadRequest& request);
    QueryResponse deserialize_query_response(const std::string& response_data);
    bool deserialize_bool_response(const std::string& response_data);
};

} // namespace dann
