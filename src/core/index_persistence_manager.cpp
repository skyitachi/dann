#include "dann/index_persistence_manager.h"
#include "dann/distributed_index_ivf.h"
#include "dann/logger.h"
#include <filesystem>

namespace dann {

IndexPersistenceManager::IndexPersistenceManager(DistributedIndexIVF* index, const PersistenceConfig& config)
    : index_(index), config_(config) {}

IndexPersistenceManager::~IndexPersistenceManager() {
    stop();
}

void IndexPersistenceManager::start() {
    if (running_.load()) {
        return;
    }
    
    if (!config_.enabled) {
        LOG_INFO("Persistence manager is disabled");
        return;
    }
    
    stop_requested_.store(false);
    running_.store(true);
    save_thread_ = std::make_unique<std::thread>(&IndexPersistenceManager::auto_save_loop, this);
    
    LOG_INFOF("Persistence manager started with interval %d seconds", config_.save_interval_seconds);
}

void IndexPersistenceManager::stop() {
    if (!running_.load()) {
        return;
    }
    
    stop_requested_.store(true);
    cv_.notify_all();
    
    if (save_thread_ && save_thread_->joinable()) {
        save_thread_->join();
    }
    
    running_.store(false);
    LOG_INFO("Persistence manager stopped");
}

void IndexPersistenceManager::force_save() {
    force_save_requested_.store(true);
    cv_.notify_all();
}

void IndexPersistenceManager::set_dirty(bool dirty) {
    if (index_) {
        index_->set_dirty(dirty);
    }
}

void IndexPersistenceManager::auto_save_loop() {
    while (!stop_requested_.load()) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::seconds(config_.save_interval_seconds), [this] {
            return stop_requested_.load() || force_save_requested_.load();
        });
        
        if (stop_requested_.load()) {
            if (index_ && index_->is_dirty()) {
                LOG_INFO("Performing final save before shutdown...");
                if (index_->save_index(config_.index_path)) {
                    LOG_INFO("Final save completed successfully");
                } else {
                    LOG_ERROR("Final save failed");
                }
            }
            break;
        }
        
        force_save_requested_.store(false);
        
        if (index_ && index_->is_dirty()) {
            LOG_INFO("Auto-saving dirty index...");
            if (index_->save_index(config_.index_path)) {
                LOG_INFO("Auto-save completed successfully");
            } else {
                LOG_ERROR("Auto-save failed");
            }
        }
    }
}

} // namespace dann
