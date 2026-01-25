#include "dann/logger.h"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <thread>

namespace dann {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() 
    : level_(LogLevel::INFO), console_output_(true), max_file_size_(100 * 1024 * 1024),
      max_files_(5), pattern_("[%Y-%m-%d %H:%M:%S] [%l] %v"), initialized_(false) {
    
    stats_ = LogStats{};
    stats_.total_messages = 0;
    for (int i = 0; i < 6; ++i) {
        stats_.messages_by_level[i] = 0;
    }
    stats_.bytes_written = 0;
    stats_.file_rotations = 0;
}

Logger::~Logger() {
    close();
}

void Logger::set_level(LogLevel level) {
    level_ = level;
}

void Logger::set_output_file(const std::string& filename) {
    output_file_ = filename;
    
    if (initialized_.load()) {
        close_file();
        open_file();
    }
}

void Logger::set_console_output(bool enabled) {
    console_output_ = enabled;
}

void Logger::set_max_file_size(size_t max_size_mb) {
    max_file_size_ = max_size_mb * 1024 * 1024;
}

void Logger::set_max_files(int max_files) {
    max_files_ = std::max(1, max_files);
}

void Logger::set_pattern(const std::string& pattern) {
    pattern_ = pattern;
}

void Logger::trace(const std::string& message) {
    if (level_ <= LogLevel::TRACE) {
        log(LogLevel::TRACE, message);
    }
}

void Logger::debug(const std::string& message) {
    if (level_ <= LogLevel::DEBUG) {
        log(LogLevel::DEBUG, message);
    }
}

void Logger::info(const std::string& message) {
    if (level_ <= LogLevel::INFO) {
        log(LogLevel::INFO, message);
    }
}

void Logger::warn(const std::string& message) {
    if (level_ <= LogLevel::WARN) {
        log(LogLevel::WARN, message);
    }
}

void Logger::error(const std::string& message) {
    if (level_ <= LogLevel::ERROR) {
        log(LogLevel::ERROR, message);
    }
}

void Logger::fatal(const std::string& message) {
    if (level_ <= LogLevel::FATAL) {
        log(LogLevel::FATAL, message);
    }
}

void Logger::flush() {
    if (file_stream_ && file_stream_->is_open()) {
        file_stream_->flush();
    }
    
    std::cout.flush();
    std::cerr.flush();
}

void Logger::close() {
    close_file();
    initialized_ = false;
}

Logger::LogStats Logger::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void Logger::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = LogStats{};
    stats_.total_messages = 0;
    for (int i = 0; i < 6; ++i) {
        stats_.messages_by_level[i] = 0;
    }
    stats_.bytes_written = 0;
    stats_.file_rotations = 0;
}

void Logger::log(LogLevel level, const std::string& message) {
    if (!initialized_.load()) {
        // Auto-initialize
        open_file();
        initialized_ = true;
    }
    
    std::string formatted_message = format_message(level, message);
    
    // Write to file
    write_to_file(level, formatted_message);
    
    // Write to console
    write_to_console(level, formatted_message);
    
    // Update stats
    update_stats(level, formatted_message.size());
}

void Logger::write_to_file(LogLevel level, const std::string& formatted_message) {
    if (!output_file_.empty()) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        
        if (!file_stream_ || !file_stream_->is_open()) {
            open_file();
        }
        
        if (file_stream_ && file_stream_->is_open()) {
            *file_stream_ << formatted_message << std::endl;
            
            // Check if rotation is needed
            if (file_stream_->tellp() > static_cast<std::streampos>(max_file_size_)) {
                rotate_files();
            }
        }
    }
}

void Logger::write_to_console(LogLevel level, const std::string& formatted_message) {
    if (console_output_) {
        if (level <= LogLevel::INFO) {
            std::cout << formatted_message << std::endl;
        } else {
            std::cerr << formatted_message << std::endl;
        }
    }
}

void Logger::rotate_file_if_needed() {
    if (file_stream_ && file_stream_->is_open()) {
        if (file_stream_->tellp() > static_cast<std::streampos>(max_file_size_)) {
            rotate_files();
        }
    }
}

std::string Logger::format_message(LogLevel level, const std::string& message) {
    std::string formatted = pattern_;
    
    // Replace placeholders
    size_t pos = formatted.find("%Y");
    if (pos != std::string::npos) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t);
        
        char buffer[256];
        std::strftime(buffer, sizeof(buffer), "%Y", &tm);
        formatted.replace(pos, 2, buffer);
    }
    
    pos = formatted.find("%m");
    if (pos != std::string::npos) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t);
        
        char buffer[256];
        std::strftime(buffer, sizeof(buffer), "%m", &tm);
        formatted.replace(pos, 2, buffer);
    }
    
    pos = formatted.find("%d");
    if (pos != std::string::npos) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t);
        
        char buffer[256];
        std::strftime(buffer, sizeof(buffer), "%d", &tm);
        formatted.replace(pos, 2, buffer);
    }
    
    pos = formatted.find("%H");
    if (pos != std::string::npos) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t);
        
        char buffer[256];
        std::strftime(buffer, sizeof(buffer), "%H", &tm);
        formatted.replace(pos, 2, buffer);
    }
    
    pos = formatted.find("%M");
    if (pos != std::string::npos) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t);
        
        char buffer[256];
        std::strftime(buffer, sizeof(buffer), "%M", &tm);
        formatted.replace(pos, 2, buffer);
    }
    
    pos = formatted.find("%S");
    if (pos != std::string::npos) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t);
        
        char buffer[256];
        std::strftime(buffer, sizeof(buffer), "%S", &tm);
        formatted.replace(pos, 2, buffer);
    }
    
    pos = formatted.find("%l");
    if (pos != std::string::npos) {
        formatted.replace(pos, 2, level_to_string(level));
    }
    
    pos = formatted.find("%v");
    if (pos != std::string::npos) {
        formatted.replace(pos, 2, message);
    }
    
    pos = formatted.find("%t");
    if (pos != std::string::npos) {
        formatted.replace(pos, 2, get_thread_id());
    }
    
    return formatted;
}

std::string Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm = *std::localtime(&time_t);
    
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string Logger::get_thread_id() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

void Logger::open_file() {
    if (output_file_.empty()) {
        return;
    }
    
    try {
        // Create directory if needed
        std::filesystem::path file_path(output_file_);
        std::filesystem::create_directories(file_path.parent_path());
        
        file_stream_ = std::make_unique<std::ofstream>(output_file_, std::ios::app);
        if (!file_stream_->is_open()) {
            file_stream_.reset();
        }
    } catch (const std::exception& e) {
        file_stream_.reset();
    }
}

void Logger::close_file() {
    if (file_stream_ && file_stream_->is_open()) {
        file_stream_->close();
    }
    file_stream_.reset();
}

void Logger::rotate_files() {
    if (output_file_.empty()) {
        return;
    }
    
    close_file();
    
    // Move current file to .1
    std::string file1 = output_file_ + ".1";
    std::filesystem::rename(output_file_, file1);
    
    // Move existing files
    for (int i = max_files_ - 1; i >= 1; --i) {
        std::string current_file = output_file_ + "." + std::to_string(i);
        std::string next_file = output_file_ + "." + std::to_string(i + 1);
        
        if (std::filesystem::exists(current_file)) {
            if (i == max_files_ - 1) {
                std::filesystem::remove(current_file);
            } else {
                std::filesystem::rename(current_file, next_file);
            }
        }
    }
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.file_rotations++;
    }
    
    // Open new file
    open_file();
}

std::string Logger::get_rotation_filename(int index) {
    if (index == 0) {
        return output_file_;
    }
    return output_file_ + "." + std::to_string(index);
}

void Logger::cleanup_old_files() {
    // Remove files beyond max_files_
    for (int i = max_files_; i <= max_files_ + 10; ++i) {
        std::string filename = get_rotation_filename(i);
        if (std::filesystem::exists(filename)) {
            std::filesystem::remove(filename);
        } else {
            break;
        }
    }
}

void Logger::update_stats(LogLevel level, size_t message_size) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_messages++;
    stats_.messages_by_level[static_cast<int>(level)]++;
    stats_.bytes_written += message_size;
}

} // namespace dann
