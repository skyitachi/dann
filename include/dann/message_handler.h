#pragma once

#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <queue>
#include <atomic>
#include <condition_variable>
#include "dann/types.h"

namespace dann {

enum class MessageType {
    SEARCH_REQUEST,
    SEARCH_RESPONSE,
    ADD_VECTORS_REQUEST,
    ADD_VECTORS_RESPONSE,
    REMOVE_VECTOR_REQUEST,
    REMOVE_VECTOR_RESPONSE,
    UPDATE_VECTOR_REQUEST,
    UPDATE_VECTOR_RESPONSE,
    HEARTBEAT,
    NODE_JOIN,
    NODE_LEAVE,
    CONFLICT_RESOLUTION,
    ANTI_ENTROPY,
    ERROR
};

struct Message {
    MessageType type;
    std::string sender_id;
    std::string receiver_id;
    std::string data;
    uint64_t timestamp;
    uint64_t message_id;
    
    Message(MessageType t, const std::string& sender, const std::string& receiver, 
            const std::string& msg_data)
        : type(t), sender_id(sender), receiver_id(receiver), data(msg_data),
          timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch()).count()),
          message_id(generate_message_id()) {}
    
private:
    static uint64_t generate_message_id();
};

class MessageHandler {
public:
    MessageHandler(const std::string& node_id);
    ~MessageHandler();
    
    // Message handling
    using MessageCallback = std::function<void(const Message&)>;
    
    void register_handler(MessageType type, MessageCallback callback);
    void unregister_handler(MessageType type);
    
    // Message sending
    bool send_message(const Message& message);
    bool broadcast_message(const Message& message);
    
    // Message processing
    void start_processing();
    void stop_processing();
    
    // Queue management
    size_t queue_size() const;
    void clear_queue();
    
    // Configuration
    void set_max_queue_size(size_t max_size);
    void set_processing_threads(size_t num_threads);
    
    // Metrics
    struct HandlerMetrics {
        uint64_t messages_sent;
        uint64_t messages_received;
        uint64_t messages_processed;
        uint64_t messages_dropped;
        uint64_t processing_errors;
        double avg_processing_time_ms;
        std::unordered_map<MessageType, uint64_t> message_counts;
    };
    
    HandlerMetrics get_metrics() const;
    void reset_metrics();
    
private:
    std::string node_id_;
    std::atomic<bool> processing_;
    size_t max_queue_size_;
    size_t num_processing_threads_;
    
    mutable std::mutex handlers_mutex_;
    std::unordered_map<MessageType, MessageCallback> handlers_;
    
    mutable std::mutex queue_mutex_;
    std::queue<Message> message_queue_;
    std::condition_variable queue_cv_;
    
    std::vector<std::thread> processing_threads_;
    
    mutable std::mutex metrics_mutex_;
    HandlerMetrics metrics_;
    
    // Processing
    void processing_loop();
    void process_message(const Message& message);
    
    // Message routing
    bool route_message(const Message& message);
    std::string get_destination_address(const std::string& node_id);
    
    // Serialization
    std::string serialize_message(const Message& message);
    Message deserialize_message(const std::string& data);
    
    // Metrics
    void update_metrics(const Message& message, bool processed, double processing_time);
    
    // Worker threads
    void start_processing_threads();
    void stop_processing_threads();
};

} // namespace dann
