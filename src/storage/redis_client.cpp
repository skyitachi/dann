#include "dann/redis_client.h"
#include <hiredis/hiredis.h>
#include <chrono>
#include <thread>
#include <cstring>

namespace dann {

RedisClient::RedisClient(const std::string& host, int port, int db)
    : host_(host), port_(port), db_(db), connected_(false),
      timeout_ms_(5000), max_retries_(3), pool_size_(10) {
    
    redis_context_ = nullptr;
    redis_reply_ = nullptr;
    
    metrics_ = RedisMetrics{};
    metrics_.commands_sent = 0;
    metrics_.commands_succeeded = 0;
    metrics_.commands_failed = 0;
    metrics_.connection_errors = 0;
    metrics_.timeout_errors = 0;
    metrics_.avg_response_time_ms = 0.0;
    metrics_.bytes_sent = 0;
    metrics_.bytes_received = 0;
}

RedisClient::~RedisClient() {
    disconnect();
}

bool RedisClient::connect() {
    if (connected_.load()) {
        return true;
    }
    
    return create_connection();
}

bool RedisClient::disconnect() {
    if (!connected_.load()) {
        return true;
    }
    
    connected_ = false;
    close_connection();
    
    return true;
}

bool RedisClient::is_connected() const {
    return connected_.load();
}

bool RedisClient::set(const std::string& key, const std::string& value) {
    std::string command = "SET " + key + " " + value;
    return execute_command(command);
}

std::string RedisClient::get(const std::string& key) {
    std::string command = "GET " + key;
    return execute_command_with_reply(command);
}

bool RedisClient::del(const std::string& key) {
    std::string command = "DEL " + key;
    return execute_command(command);
}

bool RedisClient::exists(const std::string& key) {
    std::string command = "EXISTS " + key;
    std::string reply = execute_command_with_reply(command);
    return reply == "1";
}

bool RedisClient::set_vector(const std::string& key, const std::vector<float>& vector) {
    std::string serialized = serialize_vector(vector);
    return set(key, serialized);
}

std::vector<float> RedisClient::get_vector(const std::string& key) {
    std::string data = get(key);
    if (data.empty()) {
        return {};
    }
    
    return deserialize_vector(data);
}

bool RedisClient::del_vector(const std::string& key) {
    return del(key);
}

bool RedisClient::mset(const std::vector<std::pair<std::string, std::string>>& key_values) {
    std::string command = "MSET";
    for (const auto& kv : key_values) {
        command += " " + kv.first + " " + kv.second;
    }
    return execute_command(command);
}

std::vector<std::string> RedisClient::mget(const std::vector<std::string>& keys) {
    std::string command = "MGET";
    for (const auto& key : keys) {
        command += " " + key;
    }
    
    std::string reply = execute_command_with_reply(command);
    // Parse array response - simplified
    return {reply};
}

bool RedisClient::lpush(const std::string& key, const std::string& value) {
    std::string command = "LPUSH " + key + " " + value;
    return execute_command(command);
}

bool RedisClient::rpush(const std::string& key, const std::string& value) {
    std::string command = "RPUSH " + key + " " + value;
    return execute_command(command);
}

std::string RedisClient::lpop(const std::string& key) {
    std::string command = "LPOP " + key;
    return execute_command_with_reply(command);
}

std::string RedisClient::rpop(const std::string& key) {
    std::string command = "RPOP " + key;
    return execute_command_with_reply(command);
}

std::vector<std::string> RedisClient::lrange(const std::string& key, int start, int stop) {
    std::string command = "LRANGE " + key + " " + std::to_string(start) + " " + std::to_string(stop);
    std::string reply = execute_command_with_reply(command);
    // Parse array response - simplified
    return {reply};
}

size_t RedisClient::llen(const std::string& key) {
    std::string command = "LLEN " + key;
    std::string reply = execute_command_with_reply(command);
    return std::stoull(reply);
}

bool RedisClient::hset(const std::string& key, const std::string& field, const std::string& value) {
    std::string command = "HSET " + key + " " + field + " " + value;
    return execute_command(command);
}

std::string RedisClient::hget(const std::string& key, const std::string& field) {
    std::string command = "HGET " + key + " " + field;
    return execute_command_with_reply(command);
}

bool RedisClient::hdel(const std::string& key, const std::string& field) {
    std::string command = "HDEL " + key + " " + field;
    return execute_command(command);
}

std::vector<std::string> RedisClient::hkeys(const std::string& key) {
    std::string command = "HKEYS " + key;
    std::string reply = execute_command_with_reply(command);
    // Parse array response - simplified
    return {reply};
}

std::vector<std::string> RedisClient::hvals(const std::string& key) {
    std::string command = "HVALS " + key;
    std::string reply = execute_command_with_reply(command);
    // Parse array response - simplified
    return {reply};
}

bool RedisClient::sadd(const std::string& key, const std::string& member) {
    std::string command = "SADD " + key + " " + member;
    return execute_command(command);
}

bool RedisClient::srem(const std::string& key, const std::string& member) {
    std::string command = "SREM " + key + " " + member;
    return execute_command(command);
}

std::vector<std::string> RedisClient::smembers(const std::string& key) {
    std::string command = "SMEMBERS " + key;
    std::string reply = execute_command_with_reply(command);
    // Parse array response - simplified
    return {reply};
}

bool RedisClient::sismember(const std::string& key, const std::string& member) {
    std::string command = "SISMEMBER " + key + " " + member;
    std::string reply = execute_command_with_reply(command);
    return reply == "1";
}

bool RedisClient::publish(const std::string& channel, const std::string& message) {
    std::string command = "PUBLISH " + channel + " " + message;
    return execute_command(command);
}

bool RedisClient::subscribe(const std::string& channel, std::function<void(const std::string&, const std::string&)> callback) {
    // In a real implementation, this would set up a subscription loop
    // For now, just return true
    return true;
}

bool RedisClient::unsubscribe(const std::string& channel) {
    std::string command = "UNSUBSCRIBE " + channel;
    return execute_command(command);
}

void RedisClient::multi() {
    execute_command("MULTI");
}

bool RedisClient::exec() {
    std::string reply = execute_command_with_reply("EXEC");
    return !reply.empty();
}

void RedisClient::discard() {
    execute_command("DISCARD");
}

bool RedisClient::expire(const std::string& key, int seconds) {
    std::string command = "EXPIRE " + key + " " + std::to_string(seconds);
    return execute_command(command);
}

bool RedisClient::persist(const std::string& key) {
    std::string command = "PERSIST " + key;
    return execute_command(command);
}

int RedisClient::ttl(const std::string& key) {
    std::string command = "TTL " + key;
    std::string reply = execute_command_with_reply(command);
    return std::stoi(reply);
}

std::vector<std::string> RedisClient::cluster_nodes() {
    std::string command = "CLUSTER NODES";
    std::string reply = execute_command_with_reply(command);
    // Parse response - simplified
    return {reply};
}

std::string RedisClient::cluster_info() {
    return execute_command_with_reply("CLUSTER INFO");
}

bool RedisClient::cluster_save() {
    return execute_command("CLUSTER SAVE");
}

void RedisClient::set_timeout_ms(int timeout_ms) {
    timeout_ms_ = std::max(100, timeout_ms);
}

void RedisClient::set_max_retries(int max_retries) {
    max_retries_ = std::max(0, max_retries);
}

void RedisClient::set_pool_size(size_t pool_size) {
    pool_size_ = std::max(size_t(1), pool_size);
}

bool RedisClient::ping() {
    std::string reply = execute_command_with_reply("PING");
    return reply == "PONG";
}

std::string RedisClient::info() {
    return execute_command_with_reply("INFO");
}

RedisClient::RedisMetrics RedisClient::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}

void RedisClient::reset_metrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_ = RedisMetrics{};
    metrics_.commands_sent = 0;
    metrics_.commands_succeeded = 0;
    metrics_.commands_failed = 0;
    metrics_.connection_errors = 0;
    metrics_.timeout_errors = 0;
    metrics_.avg_response_time_ms = 0.0;
    metrics_.bytes_sent = 0;
    metrics_.bytes_received = 0;
}

bool RedisClient::create_connection() {
    try {
        redis_context_ = redisConnect(host_.c_str(), port_);
        
        if (!redis_context_ || redis_context_->err) {
            if (redis_context_) {
                redisFree(redis_context_);
                redis_context_ = nullptr;
            }
            return false;
        }
        
        // Select database
        if (db_ > 0) {
            redisReply* reply = static_cast<redisReply*>(redisCommand(redis_context_, "SELECT %d", db_));
            if (reply) {
                freeReplyObject(reply);
            }
        }
        
        connected_ = true;
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

void RedisClient::close_connection() {
    if (redis_context_) {
        redisFree(redis_context_);
        redis_context_ = nullptr;
    }
}

bool RedisClient::reconnect() {
    close_connection();
    return create_connection();
}

bool RedisClient::execute_command(const std::string& command) {
    auto start_time = std::chrono::high_resolution_clock::now();
    bool success = false;
    
    try {
        if (!connected_.load() && !reconnect()) {
            update_metrics(false, 0, command.size(), 0);
            return false;
        }
        
        redisReply* reply = static_cast<redisReply*>(redisCommand(redis_context_, command.c_str()));
        
        if (reply) {
            success = (reply->type != REDIS_REPLY_ERROR);
            freeReplyObject(reply);
        } else {
            success = false;
            // Connection might be lost
            connected_ = false;
        }
        
    } catch (const std::exception& e) {
        success = false;
        connected_ = false;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    update_metrics(success, response_time.count(), command.size(), 0);
    
    return success;
}

std::string RedisClient::execute_command_with_reply(const std::string& command) {
    auto start_time = std::chrono::high_resolution_clock::now();
    std::string reply_str;
    bool success = false;
    
    try {
        if (!connected_.load() && !reconnect()) {
            update_metrics(false, 0, command.size(), 0);
            return "";
        }
        
        redisReply* reply = static_cast<redisReply*>(redisCommand(redis_context_, command.c_str()));
        
        if (reply) {
            success = (reply->type != REDIS_REPLY_ERROR);
            
            if (success && reply->type == REDIS_REPLY_STRING) {
                reply_str = std::string(reply->str, reply->len);
            } else if (success && reply->type == REDIS_REPLY_INTEGER) {
                reply_str = std::to_string(reply->integer);
            }
            
            freeReplyObject(reply);
        } else {
            success = false;
            connected_ = false;
        }
        
    } catch (const std::exception& e) {
        success = false;
        connected_ = false;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    update_metrics(success, response_time.count(), command.size(), reply_str.size());
    
    return success ? reply_str : "";
}

void RedisClient::update_metrics(bool success, double response_time, size_t bytes_sent, size_t bytes_received) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    metrics_.commands_sent++;
    
    if (success) {
        metrics_.commands_succeeded++;
    } else {
        metrics_.commands_failed++;
    }
    
    metrics_.bytes_sent += bytes_sent;
    metrics_.bytes_received += bytes_received;
    
    // Update average response time
    double total_time = metrics_.avg_response_time_ms * metrics_.commands_sent;
    metrics_.avg_response_time_ms = (total_time + response_time) / metrics_.commands_sent;
}

std::string RedisClient::serialize_vector(const std::vector<float>& vector) {
    std::string result;
    result.reserve(vector.size() * sizeof(float));
    
    for (float value : vector) {
        const char* bytes = reinterpret_cast<const char*>(&value);
        result.append(bytes, sizeof(float));
    }
    
    return result;
}

std::vector<float> RedisClient::deserialize_vector(const std::string& data) {
    std::vector<float> vector;
    
    if (data.size() % sizeof(float) != 0) {
        return vector;
    }
    
    size_t num_floats = data.size() / sizeof(float);
    vector.resize(num_floats);
    
    const float* floats = reinterpret_cast<const float*>(data.data());
    for (size_t i = 0; i < num_floats; ++i) {
        vector[i] = floats[i];
    }
    
    return vector;
}

std::string RedisClient::build_command(const std::vector<std::string>& args) {
    std::string command;
    for (const auto& arg : args) {
        if (!command.empty()) {
            command += " ";
        }
        command += arg;
    }
    return command;
}

} // namespace dann
