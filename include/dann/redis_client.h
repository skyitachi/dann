#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <functional>
#include "dann/types.h"

namespace dann {

class RedisClient {
public:
    RedisClient(const std::string& host = "localhost", int port = 6379, int db = 0);
    ~RedisClient();
    
    // Connection management
    bool connect();
    bool disconnect();
    bool is_connected() const;
    
    // Basic operations
    bool set(const std::string& key, const std::string& value);
    std::string get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);
    
    // Vector operations
    bool set_vector(const std::string& key, const std::vector<float>& vector);
    std::vector<float> get_vector(const std::string& key);
    bool del_vector(const std::string& key);
    
    // Batch operations
    bool mset(const std::vector<std::pair<std::string, std::string>>& key_values);
    std::vector<std::string> mget(const std::vector<std::string>& keys);
    
    // List operations
    bool lpush(const std::string& key, const std::string& value);
    bool rpush(const std::string& key, const std::string& value);
    std::string lpop(const std::string& key);
    std::string rpop(const std::string& key);
    std::vector<std::string> lrange(const std::string& key, int start, int stop);
    size_t llen(const std::string& key);
    
    // Hash operations
    bool hset(const std::string& key, const std::string& field, const std::string& value);
    std::string hget(const std::string& key, const std::string& field);
    bool hdel(const std::string& key, const std::string& field);
    std::vector<std::string> hkeys(const std::string& key);
    std::vector<std::string> hvals(const std::string& key);
    
    // Set operations
    bool sadd(const std::string& key, const std::string& member);
    bool srem(const std::string& key, const std::string& member);
    std::vector<std::string> smembers(const std::string& key);
    bool sismember(const std::string& key, const std::string& member);
    
    // Pub/Sub operations
    bool publish(const std::string& channel, const std::string& message);
    bool subscribe(const std::string& channel, std::function<void(const std::string&, const std::string&)> callback);
    bool unsubscribe(const std::string& channel);
    
    // Transaction operations
    void multi();
    bool exec();
    void discard();
    
    // TTL operations
    bool expire(const std::string& key, int seconds);
    bool persist(const std::string& key);
    int ttl(const std::string& key);
    
    // Cluster operations
    std::vector<std::string> cluster_nodes();
    std::string cluster_info();
    bool cluster_save();
    
    // Configuration
    void set_timeout_ms(int timeout_ms);
    void set_max_retries(int max_retries);
    void set_pool_size(size_t pool_size);
    
    // Health check
    bool ping();
    std::string info();
    
    // Metrics
    struct RedisMetrics {
        uint64_t commands_sent;
        uint64_t commands_succeeded;
        uint64_t commands_failed;
        uint64_t connection_errors;
        uint64_t timeout_errors;
        double avg_response_time_ms;
        uint64_t bytes_sent;
        uint64_t bytes_received;
    };
    
    RedisMetrics get_metrics() const;
    void reset_metrics();
    
private:
    std::string host_;
    int port_;
    int db_;
    std::atomic<bool> connected_;
    int timeout_ms_;
    int max_retries_;
    size_t pool_size_;
    
    void* redis_context_; // redisContext*
    void* redis_reply_;   // redisReply*
    
    mutable std::mutex metrics_mutex_;
    RedisMetrics metrics_;
    
    // Connection management
    bool create_connection();
    void close_connection();
    bool reconnect();
    
    // Command execution
    bool execute_command(const std::string& command);
    std::string execute_command_with_reply(const std::string& command);
    
    // Error handling
    bool handle_error();
    bool retry_command(const std::string& command);
    
    // Serialization
    std::string serialize_vector(const std::vector<float>& vector);
    std::vector<float> deserialize_vector(const std::string& data);
    
    // Metrics
    void update_metrics(bool success, double response_time, size_t bytes_sent, size_t bytes_received);
    
    // Utilities
    std::string build_command(const std::vector<std::string>& args);
    std::vector<std::string> parse_reply_array(const std::string& reply);
};

} // namespace dann
