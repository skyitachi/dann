#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace dann {

class DistributedIndexIVF;

struct PersistenceConfig {
    bool enabled = true;
    int save_interval_seconds = 60;
    std::string index_path = "./data";
    
    PersistenceConfig() = default;
    PersistenceConfig(bool en, int interval, const std::string& path)
        : enabled(en), save_interval_seconds(interval), index_path(path) {}
};

class IndexPersistenceManager {
public:
    using SaveCallback = std::function<bool(const std::string&)>;
    using IsDirtyCallback = std::function<bool()>;
    
    IndexPersistenceManager(DistributedIndexIVF* index, const PersistenceConfig& config);
    ~IndexPersistenceManager();
    
    void start();
    void stop();
    bool is_running() const { return running_.load(); }
    
    void force_save();
    void set_dirty(bool dirty);
    
    const PersistenceConfig& config() const { return config_; }
    
private:
    void auto_save_loop();
    
    DistributedIndexIVF* index_;
    PersistenceConfig config_;
    
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> force_save_requested_{false};
    
    std::unique_ptr<std::thread> save_thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

} // namespace dann
