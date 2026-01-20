#include "dann/message_handler.h"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <openssl/md5.h>

namespace dann {

uint64_t Message::generate_message_id() {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    return (timestamp << 20) | (counter++ & 0xFFFFF);
}

MessageHandler::MessageHandler(const std::string& node_id)
    : node_id_(node_id), processing_(false), max_queue_size_(10000), 
      num_processing_threads_(4) {
    
    metrics_ = HandlerMetrics{};
    metrics_.messages_sent = 0;
    metrics_.messages_received = 0;
    metrics_.messages_processed = 0;
    metrics_.messages_dropped = 0;
    metrics_.processing_errors = 0;
    metrics_.avg_processing_time_ms = 0.0;
}

MessageHandler::~MessageHandler() {
    stop_processing();
}

void MessageHandler::register_handler(MessageType type, MessageCallback callback) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_[type] = callback;
}

void MessageHandler::unregister_handler(MessageType type) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_.erase(type);
}

bool MessageHandler::send_message(const Message& message) {
    try {
        std::string serialized = serialize_message(message);
        
        // Route message to destination
        bool sent = route_message(message);
        
        if (sent) {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            metrics_.messages_sent++;
            metrics_.message_counts[message.type]++;
        }
        
        return sent;
    } catch (const std::exception& e) {
        return false;
    }
}

bool MessageHandler::broadcast_message(const Message& message) {
    // In a real implementation, this would send to all nodes in the cluster
    // For now, just mark as sent
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.messages_sent++;
    metrics_.message_counts[message.type]++;
    
    return true;
}

void MessageHandler::start_processing() {
    if (processing_.load()) {
        return;
    }
    
    processing_ = true;
    start_processing_threads();
}

void MessageHandler::stop_processing() {
    if (!processing_.load()) {
        return;
    }
    
    processing_ = false;
    queue_cv_.notify_all();
    
    stop_processing_threads();
}

size_t MessageHandler::queue_size() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return message_queue_.size();
}

void MessageHandler::clear_queue() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!message_queue_.empty()) {
        message_queue_.pop();
    }
}

void MessageHandler::set_max_queue_size(size_t max_size) {
    max_queue_size_ = std::max(size_t(1), max_size);
}

void MessageHandler::set_processing_threads(size_t num_threads) {
    num_processing_threads_ = std::max(size_t(1), num_threads);
}

MessageHandler::HandlerMetrics MessageHandler::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}

void MessageHandler::reset_metrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_ = HandlerMetrics{};
    metrics_.messages_sent = 0;
    metrics_.messages_received = 0;
    metrics_.messages_processed = 0;
    metrics_.messages_dropped = 0;
    metrics_.processing_errors = 0;
    metrics_.avg_processing_time_ms = 0.0;
}

void MessageHandler::processing_loop() {
    while (processing_.load()) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        queue_cv_.wait(lock, [this] {
            return !message_queue_.empty() || !processing_.load();
        });
        
        if (!processing_.load()) {
            break;
        }
        
        if (message_queue_.empty()) {
            continue;
        }
        
        Message message = message_queue_.front();
        message_queue_.pop();
        lock.unlock();
        
        process_message(message);
    }
}

void MessageHandler::process_message(const Message& message) {
    auto start_time = std::chrono::high_resolution_clock::now();
    bool processed = false;
    
    try {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        auto it = handlers_.find(message.type);
        
        if (it != handlers_.end()) {
            // Call the handler
            it->second(message);
            processed = true;
        } else {
            // No handler registered for this message type
            processed = false;
        }
        
    } catch (const std::exception& e) {
        processed = false;
        
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.processing_errors++;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    update_metrics(message, processed, processing_time.count());
}

bool MessageHandler::route_message(const Message& message) {
    // In a real implementation, this would route the message to the appropriate node
    // For now, just simulate successful routing
    return true;
}

std::string MessageHandler::get_destination_address(const std::string& node_id) {
    // In a real implementation, this would look up the node's address
    // For now, return a placeholder
    return node_id + ":8080";
}

std::string MessageHandler::serialize_message(const Message& message) {
    std::stringstream ss;
    
    ss << std::hex << std::setfill('0');
    ss << "msg:" << static_cast<int>(message.type) << ":";
    ss << message.sender_id << ":" << message.receiver_id << ":";
    ss << message.timestamp << ":" << message.message_id << ":";
    ss << message.data.size() << ":" << message.data;
    
    return ss.str();
}

Message MessageHandler::deserialize_message(const std::string& data) {
    // Simplified deserialization - in real implementation would be more robust
    Message message(MessageType::ERROR, "", "", "");
    
    // Parse the serialized message
    // This is a very basic implementation
    size_t pos = 0;
    
    if (data.substr(pos, 4) == "msg:") {
        pos += 4;
        
        // Extract message type
        size_t next_pos = data.find(':', pos);
        if (next_pos != std::string::npos) {
            int type = std::stoi(data.substr(pos, next_pos - pos));
            message.type = static_cast<MessageType>(type);
            pos = next_pos + 1;
            
            // Extract sender ID
            next_pos = data.find(':', pos);
            if (next_pos != std::string::npos) {
                message.sender_id = data.substr(pos, next_pos - pos);
                pos = next_pos + 1;
                
                // Extract receiver ID
                next_pos = data.find(':', pos);
                if (next_pos != std::string::npos) {
                    message.receiver_id = data.substr(pos, next_pos - pos);
                    pos = next_pos + 1;
                    
                    // Extract timestamp
                    next_pos = data.find(':', pos);
                    if (next_pos != std::string::npos) {
                        message.timestamp = std::stoull(data.substr(pos, next_pos - pos));
                        pos = next_pos + 1;
                        
                        // Extract message ID
                        next_pos = data.find(':', pos);
                        if (next_pos != std::string::npos) {
                            message.message_id = std::stoull(data.substr(pos, next_pos - pos));
                            pos = next_pos + 1;
                            
                            // Extract data length and data
                            next_pos = data.find(':', pos);
                            if (next_pos != std::string::npos) {
                                size_t data_length = std::stoull(data.substr(pos, next_pos - pos));
                                pos = next_pos + 1;
                                
                                if (pos + data_length <= data.length()) {
                                    message.data = data.substr(pos, data_length);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    return message;
}

void MessageHandler::update_metrics(const Message& message, bool processed, double processing_time) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    metrics_.messages_received++;
    
    if (processed) {
        metrics_.messages_processed++;
    } else {
        metrics_.messages_dropped++;
    }
    
    // Update average processing time
    double total_time = metrics_.avg_processing_time_ms * metrics_.messages_received;
    metrics_.avg_processing_time_ms = (total_time + processing_time) / metrics_.messages_received;
    
    metrics_.message_counts[message.type]++;
}

void MessageHandler::start_processing_threads() {
    for (size_t i = 0; i < num_processing_threads_; ++i) {
        processing_threads_.emplace_back(&MessageHandler::processing_loop, this);
    }
}

void MessageHandler::stop_processing_threads() {
    for (auto& thread : processing_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    processing_threads_.clear();
}

} // namespace dann
