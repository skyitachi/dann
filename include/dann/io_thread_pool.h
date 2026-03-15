//
// IO Thread Pool for I/O-bound tasks
// Supports task submission with std::future return
//

#ifndef DANN_IO_THREAD_POOL_H
#define DANN_IO_THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <atomic>

namespace dann {

class IOThreadPool {
public:
    explicit IOThreadPool(size_t threads = 0);
    ~IOThreadPool();

    // Delete copy/move
    IOThreadPool(const IOThreadPool&) = delete;
    IOThreadPool& operator=(const IOThreadPool&) = delete;
    IOThreadPool(IOThreadPool&&) = delete;
    IOThreadPool& operator=(IOThreadPool&&) = delete;

    // Submit a task, return future
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("enqueue on stopped thread pool");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        condition_.notify_one();
        return res;
    }

    // Batch submit multiple tasks and wait for all
    template<typename F, typename... Args>
    auto enqueue_batch(size_t count, F&& f, Args&&... args) 
        -> std::vector<std::future<typename std::invoke_result<F, Args...>::type>> {
        std::vector<std::future<typename std::invoke_result<F, Args...>::type>> futures;
        futures.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            futures.push_back(enqueue(std::forward<F>(f), std::forward<Args>(args)...));
        }
        return futures;
    }

    size_t size() const { return workers_.size(); }
    size_t pending_tasks() const;
    
    // Static helper to get results from futures
    template<typename T>
    static std::vector<T> collect_results(std::vector<std::future<T>>& futures) {
        std::vector<T> results;
        results.reserve(futures.size());
        for (auto& fut : futures) {
            results.push_back(fut.get());
        }
        return results;
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
    std::atomic<size_t> pending_count_{0};
};

// Global IO thread pool getter (lazy initialization)
IOThreadPool& get_io_thread_pool(size_t threads = 0);

} // namespace dann

#endif // DANN_IO_THREAD_POOL_H
