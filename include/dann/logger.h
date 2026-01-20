#pragma once

#include <string>
#include <memory>
#include <fstream>
#include <mutex>
#include <atomic>
#include <sstream>
#include <chrono>

namespace dann {

enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
};

class Logger {
public:
    static Logger& instance();
    
    // Configuration
    void set_level(LogLevel level);
    void set_output_file(const std::string& filename);
    void set_console_output(bool enabled);
    void set_max_file_size(size_t max_size_mb);
    void set_max_files(int max_files);
    void set_pattern(const std::string& pattern);
    
    // Logging methods
    void trace(const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
    void fatal(const std::string& message);
    
    // Formatted logging
    template<typename... Args>
    void tracef(const std::string& format, Args... args);
    
    template<typename... Args>
    void debugf(const std::string& format, Args... args);
    
    template<typename... Args>
    void infof(const std::string& format, Args... args);
    
    template<typename... Args>
    void warnf(const std::string& format, Args... args);
    
    template<typename... Args>
    void errorf(const std::string& format, Args... args);
    
    template<typename... Args>
    void fatalf(const std::string& format, Args... args);
    
    // Flush and close
    void flush();
    void close();
    
    // Statistics
    struct LogStats {
        uint64_t total_messages;
        uint64_t messages_by_level[6]; // TRACE to FATAL
        uint64_t bytes_written;
        uint64_t file_rotations;
    };
    
    LogStats get_stats() const;
    void reset_stats();
    
private:
    Logger();
    ~Logger();
    
    // Prevent copying
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    LogLevel level_;
    std::string output_file_;
    bool console_output_;
    size_t max_file_size_;
    int max_files_;
    std::string pattern_;
    
    std::unique_ptr<std::ofstream> file_stream_;
    mutable std::mutex log_mutex_;
    std::atomic<bool> initialized_;
    
    mutable std::mutex stats_mutex_;
    LogStats stats_;
    
    // Internal methods
    void log(LogLevel level, const std::string& message);
    void write_to_file(LogLevel level, const std::string& formatted_message);
    void write_to_console(LogLevel level, const std::string& formatted_message);
    void rotate_file_if_needed();
    std::string format_message(LogLevel level, const std::string& message);
    std::string level_to_string(LogLevel level);
    std::string get_timestamp();
    std::string get_thread_id();
    
    // Template helper
    template<typename... Args>
    std::string format_string(const std::string& format, Args... args);
    
    // File management
    void open_file();
    void close_file();
    void rotate_files();
    std::string get_rotation_filename(int index);
    void cleanup_old_files();
    
    // Statistics
    void update_stats(LogLevel level, size_t message_size);
};

// Convenience macros
#define LOG_TRACE(msg) Logger::instance().trace(msg)
#define LOG_DEBUG(msg) Logger::instance().debug(msg)
#define LOG_INFO(msg) Logger::instance().info(msg)
#define LOG_WARN(msg) Logger::instance().warn(msg)
#define LOG_ERROR(msg) Logger::instance().error(msg)
#define LOG_FATAL(msg) Logger::instance().fatal(msg)

#define LOG_TRACEF(fmt, ...) Logger::instance().tracef(fmt, __VA_ARGS__)
#define LOG_DEBUGF(fmt, ...) Logger::instance().debugf(fmt, __VA_ARGS__)
#define LOG_INFOF(fmt, ...) Logger::instance().infof(fmt, __VA_ARGS__)
#define LOG_WARNF(fmt, ...) Logger::instance().warnf(fmt, __VA_ARGS__)
#define LOG_ERRORF(fmt, ...) Logger::instance().errorf(fmt, __VA_ARGS__)
#define LOG_FATALF(fmt, ...) Logger::instance().fatalf(fmt, __VA_ARGS__)

// Template implementations
template<typename... Args>
void Logger::tracef(const std::string& format, Args... args) {
    if (level_ <= LogLevel::TRACE) {
        log(LogLevel::TRACE, format_string(format, args...));
    }
}

template<typename... Args>
void Logger::debugf(const std::string& format, Args... args) {
    if (level_ <= LogLevel::DEBUG) {
        log(LogLevel::DEBUG, format_string(format, args...));
    }
}

template<typename... Args>
void Logger::infof(const std::string& format, Args... args) {
    if (level_ <= LogLevel::INFO) {
        log(LogLevel::INFO, format_string(format, args...));
    }
}

template<typename... Args>
void Logger::warnf(const std::string& format, Args... args) {
    if (level_ <= LogLevel::WARN) {
        log(LogLevel::WARN, format_string(format, args...));
    }
}

template<typename... Args>
void Logger::errorf(const std::string& format, Args... args) {
    if (level_ <= LogLevel::ERROR) {
        log(LogLevel::ERROR, format_string(format, args...));
    }
}

template<typename... Args>
void Logger::fatalf(const std::string& format, Args... args) {
    if (level_ <= LogLevel::FATAL) {
        log(LogLevel::FATAL, format_string(format, args...));
    }
}

template<typename... Args>
std::string Logger::format_string(const std::string& format, Args... args) {
    size_t size = snprintf(nullptr, 0, format.c_str(), args...) + 1;
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1);
}

} // namespace dann
