#include "dann/rpc_client.h"
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <chrono>
#include <thread>

namespace dann {

RPCClient::RPCClient(const std::string& address, int port)
    : address_(address), port_(port), connected_(false), 
      timeout_ms_(5000), max_retries_(3), compression_enabled_(false) {
    
    metrics_ = ClientMetrics{};
    metrics_.total_requests = 0;
    metrics_.successful_requests = 0;
    metrics_.failed_requests = 0;
    metrics_.retries = 0;
    metrics_.avg_response_time_ms = 0.0;
    metrics_.bytes_sent = 0;
    metrics_.bytes_received = 0;
}

RPCClient::~RPCClient() {
    disconnect();
}

bool RPCClient::connect() {
    if (connected_.load()) {
        return true;
    }
    
    try {
        if (create_channel()) {
            connected_ = true;
            return true;
        }
    } catch (const std::exception& e) {
        // Log error
    }
    
    return false;
}

bool RPCClient::disconnect() {
    if (!connected_.load()) {
        return true;
    }
    
    connected_ = false;
    close_channel();
    
    return true;
}

bool RPCClient::is_connected() const {
    return connected_.load();
}

std::future<QueryResponse> RPCClient::search_async(const QueryRequest& request) {
    return send_request_async<QueryRequest, QueryResponse>("Search", request);
}

QueryResponse RPCClient::search_sync(const QueryRequest& request) {
    return send_request_sync<QueryRequest, QueryResponse>("Search", request);
}

std::future<bool> RPCClient::add_vectors_async(const BulkLoadRequest& request) {
    return send_request_async<BulkLoadRequest, bool>("AddVectors", request);
}

bool RPCClient::add_vectors_sync(const BulkLoadRequest& request) {
    return send_request_sync<BulkLoadRequest, bool>("AddVectors", request);
}

std::future<bool> RPCClient::remove_vector_async(int64_t id) {
    // Create request with just the ID
    std::string request_data = std::to_string(id);
    return send_request_async<std::string, bool>("RemoveVector", request_data);
}

bool RPCClient::remove_vector_sync(int64_t id) {
    std::string request_data = std::to_string(id);
    return send_request_sync<std::string, bool>("RemoveVector", request_data);
}

std::future<bool> RPCClient::update_vector_async(int64_t id, const std::vector<float>& vector) {
    // Create request with ID and vector
    std::string request_data = std::to_string(id) + ":" + serialize_vector(vector);
    return send_request_async<std::string, bool>("UpdateVector", request_data);
}

bool RPCClient::update_vector_sync(int64_t id, const std::vector<float>& vector) {
    std::string request_data = std::to_string(id) + ":" + serialize_vector(vector);
    return send_request_sync<std::string, bool>("UpdateVector", request_data);
}

void RPCClient::set_timeout_ms(int timeout_ms) {
    timeout_ms_ = std::max(100, timeout_ms);
}

void RPCClient::set_max_retries(int max_retries) {
    max_retries_ = std::max(0, max_retries);
}

void RPCClient::enable_compression(bool enable) {
    compression_enabled_ = enable;
}

bool RPCClient::health_check() {
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Send ping request
        std::string request_data = "ping";
        std::string response_data = execute_command_with_reply("HealthCheck");
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        bool success = (response_data == "pong");
        update_metrics(success, response_time.count(), request_data.size(), response_data.size());
        
        return success;
    } catch (const std::exception& e) {
        update_metrics(false, 0, 0, 0);
        return false;
    }
}

std::string RPCClient::get_server_info() {
    try {
        return execute_command_with_reply("GetServerInfo");
    } catch (const std::exception& e) {
        return "";
    }
}

RPCClient::ClientMetrics RPCClient::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}

void RPCClient::reset_metrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_ = ClientMetrics{};
    metrics_.total_requests = 0;
    metrics_.successful_requests = 0;
    metrics_.failed_requests = 0;
    metrics_.retries = 0;
    metrics_.avg_response_time_ms = 0.0;
    metrics_.bytes_sent = 0;
    metrics_.bytes_received = 0;
}

bool RPCClient::create_channel() {
    try {
        std::string server_address = address_ + ":" + std::to_string(port_);
        
        grpc::ChannelArguments args;
        args.SetMaxReceiveMessageSize(100 * 1024 * 1024); // 100MB
        args.SetMaxSendMessageSize(100 * 1024 * 1024);    // 100MB
        
        if (compression_enabled_) {
            args.SetCompressionAlgorithm(GRPC_COMPRESS_GZIP);
        }
        
        channel_ = grpc::CreateCustomChannel(server_address, grpc::InsecureChannelCredentials(), args);
        
        if (!channel_) {
            return false;
        }
        
        // Test connection
        context_ = std::make_unique<grpc::ClientContext>();
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

void RPCClient::close_channel() {
    context_.reset();
    channel_.reset();
}

bool RPCClient::execute_command(const std::string& command) {
    // Simplified implementation - in real would use gRPC
    return true;
}

std::string RPCClient::execute_command_with_reply(const std::string& command) {
    // Simplified implementation - in real would use gRPC
    if (command == "HealthCheck") {
        return "pong";
    } else if (command == "GetServerInfo") {
        return "DANN Server v1.0.0";
    }
    return "";
}

void RPCClient::update_metrics(bool success, double response_time, size_t bytes_sent, size_t bytes_received) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    metrics_.total_requests++;
    
    if (success) {
        metrics_.successful_requests++;
    } else {
        metrics_.failed_requests++;
    }
    
    metrics_.bytes_sent += bytes_sent;
    metrics_.bytes_received += bytes_received;
    
    // Update average response time
    double total_time = metrics_.avg_response_time_ms * (metrics_.total_requests - 1);
    metrics_.avg_response_time_ms = (total_time + response_time) / metrics_.total_requests;
}

std::string RPCClient::serialize_vector(const std::vector<float>& vector) {
    std::string result;
    result.reserve(vector.size() * sizeof(float));
    
    for (float value : vector) {
        const char* bytes = reinterpret_cast<const char*>(&value);
        result.append(bytes, sizeof(float));
    }
    
    return result;
}

template<typename Request, typename Response>
std::future<Response> RPCClient::send_request_async(const std::string& method, const Request& request) {
    auto promise = std::make_shared<std::promise<Response>>();
    auto future = promise->get_future();
    
    std::thread([this, method, request, promise]() {
        try {
            Response response = send_with_retry<Request, Response>(method, request);
            promise->set_value(response);
        } catch (const std::exception& e) {
            promise->set_exception(std::current_exception());
        }
    }).detach();
    
    return future;
}

template<typename Request, typename Response>
Response RPCClient::send_request_sync(const std::string& method, const Request& request) {
    return send_with_retry<Request, Response>(method, request);
}

template<typename Request, typename Response>
Response RPCClient::send_with_retry(const std::string& method, const Request& request) {
    int attempts = 0;
    
    while (attempts <= max_retries_) {
        try {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // Serialize request
            std::string request_data = serialize_request(request);
            
            // Send request and get response
            std::string response_data = execute_command_with_reply(method);
            
            // Deserialize response
            Response response = deserialize_response<Response>(response_data);
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            update_metrics(true, response_time.count(), request_data.size(), response_data.size());
            
            return response;
            
        } catch (const std::exception& e) {
            attempts++;
            
            if (attempts > max_retries_) {
                update_metrics(false, 0, 0, 0);
                throw;
            }
            
            // Exponential backoff
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * (1 << (attempts - 1))));
        }
    }
    
    // Should not reach here
    throw std::runtime_error("Max retries exceeded");
}

std::string RPCClient::serialize_request(const QueryRequest& request) {
    // Simplified serialization - in real would use protobuf
    return "query_request_serialized";
}

std::string RPCClient::serialize_request(const BulkLoadRequest& request) {
    // Simplified serialization - in real would use protobuf
    return "bulk_load_request_serialized";
}

QueryResponse RPCClient::deserialize_query_response(const std::string& response_data) {
    // Simplified deserialization - in real would use protobuf
    QueryResponse response;
    response.success = true;
    return response;
}

bool RPCClient::deserialize_bool_response(const std::string& response_data) {
    // Simplified deserialization - in real would use protobuf
    return response_data == "true";
}

} // namespace dann
