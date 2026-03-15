//
// IO Thread Pool Implementation
//

#include "dann/io_thread_pool.h"
#include <thread>

namespace dann {

IOThreadPool::IOThreadPool(size_t threads) : stop_(false) {
    // For IO-bound tasks, we can use more threads than CPU cores
    // since threads spend most time waiting on I/O
    if (threads == 0) {
        // Use 2x hardware concurrency for IO-bound workloads
        threads = std::thread::hardware_concurrency() * 2;
        if (threads == 0) {
            threads = 4; // fallback
        }
    }

    workers_.reserve(threads);
    for (size_t i = 0; i < threads; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                    
                    if (stop_ && tasks_.empty()) {
                        return;
                    }
                    
                    task = std::move(tasks_.front());
                    tasks_.pop();
                    pending_count_--;
                }
                task();
            }
        });
    }
}

IOThreadPool::~IOThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    condition_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

size_t IOThreadPool::pending_tasks() const {
    return pending_count_.load();
}

// Global IO thread pool (lazy initialization with 2x CPU threads for IO-bound tasks)
IOThreadPool& get_io_thread_pool(size_t threads) {
    static IOThreadPool pool(threads);
    return pool;
}

} // namespace dann
