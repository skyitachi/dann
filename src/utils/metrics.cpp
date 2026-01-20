#include "dann/metrics.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace dann {

Metrics& Metrics::instance() {
    static Metrics instance;
    return instance;
}

Metrics::Metrics() : max_histogram_samples_(10000) {
    // Initialize with default histogram buckets
    histogram_buckets_ = {0.1, 0.5, 1.0, 2.5, 5.0, 10.0, 25.0, 50.0, 100.0, 250.0, 500.0, 1000.0, 2500.0, 5000.0, 10000.0};
    
    stats_ = MetricsStats{};
    stats_.total_metrics = 0;
    stats_.total_samples = 0;
    stats_.memory_usage_bytes = 0;
    stats_.avg_update_time_us = 0.0;
}

Metrics::~Metrics() = default;

void Metrics::increment_counter(const std::string& name, double value) {
    update_metric(name, MetricType::COUNTER, value);
}

void Metrics::decrement_counter(const std::string& name, double value) {
    update_metric(name, MetricType::COUNTER, -value);
}

void Metrics::set_counter(const std::string& name, double value) {
    update_metric(name, MetricType::COUNTER, value);
}

double Metrics::get_counter(const std::string& name) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end() && it->second.type == MetricType::COUNTER) {
        return it->second.value;
    }
    
    return 0.0;
}

void Metrics::set_gauge(const std::string& name, double value) {
    update_metric(name, MetricType::GAUGE, value);
}

double Metrics::get_gauge(const std::string& name) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end() && it->second.type == MetricType::GAUGE) {
        return it->second.value;
    }
    
    return 0.0;
}

void Metrics::record_histogram(const std::string& name, double value) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it == metrics_.end()) {
        metrics_[name] = MetricData(MetricType::HISTOGRAM);
        it = metrics_.find(name);
    }
    
    if (it->second.type == MetricType::HISTOGRAM) {
        add_histogram_sample(it->second, value);
    }
}

std::vector<double> Metrics::get_histogram_values(const std::string& name) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end() && it->second.type == MetricType::HISTOGRAM) {
        return it->second.histogram_values;
    }
    
    return {};
}

double Metrics::get_histogram_percentile(const std::string& name, double percentile) const {
    auto values = get_histogram_values(name);
    if (values.empty()) {
        return 0.0;
    }
    
    std::sort(values.begin(), values.end());
    
    size_t index = static_cast<size_t>(percentile / 100.0 * values.size());
    if (index >= values.size()) {
        index = values.size() - 1;
    }
    
    return values[index];
}

double Metrics::get_histogram_mean(const std::string& name) const {
    auto values = get_histogram_values(name);
    if (values.empty()) {
        return 0.0;
    }
    
    double sum = 0.0;
    for (double value : values) {
        sum += value;
    }
    
    return sum / values.size();
}

double Metrics::get_histogram_sum(const std::string& name) const {
    auto values = get_histogram_values(name);
    double sum = 0.0;
    for (double value : values) {
        sum += value;
    }
    return sum;
}

uint64_t Metrics::get_histogram_count(const std::string& name) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end() && it->second.type == MetricType::HISTOGRAM) {
        return it->second.histogram_values.size();
    }
    
    return 0;
}

std::unique_ptr<Metrics::Timer> Metrics::start_timer(const std::string& name) {
    return std::make_unique<Timer>(name);
}

void Metrics::increment_counter_with_labels(const std::string& name, 
                                         const std::unordered_map<std::string, std::string>& labels,
                                         double value) {
    std::string labeled_name = name + format_labels(labels);
    update_metric(labeled_name, MetricType::COUNTER, value);
}

void Metrics::set_gauge_with_labels(const std::string& name,
                                  const std::unordered_map<std::string, std::string>& labels,
                                  double value) {
    std::string labeled_name = name + format_labels(labels);
    update_metric(labeled_name, MetricType::GAUGE, value);
}

void Metrics::record_histogram_with_labels(const std::string& name,
                                        const std::unordered_map<std::string, std::string>& labels,
                                        double value) {
    std::string labeled_name = name + format_labels(labels);
    record_histogram(labeled_name, value);
}

void Metrics::remove_metric(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.erase(name);
}

void Metrics::clear_all_metrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.clear();
}

std::vector<std::string> Metrics::get_metric_names() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    std::vector<std::string> names;
    for (const auto& pair : metrics_) {
        names.push_back(pair.first);
    }
    
    return names;
}

std::string Metrics::export_prometheus() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    std::stringstream ss;
    
    for (const auto& pair : metrics_) {
        const std::string& name = pair.first;
        const MetricData& data = pair.second;
        
        // Add metric type
        switch (data.type) {
            case MetricType::COUNTER:
                ss << "# TYPE " << name << " counter\n";
                ss << name << " " << data.value << "\n";
                break;
                
            case MetricType::GAUGE:
                ss << "# TYPE " << name << " gauge\n";
                ss << name << " " << data.value << "\n";
                break;
                
            case MetricType::HISTOGRAM:
                ss << "# TYPE " << name << " histogram\n";
                
                // Add bucket counts
                std::vector<double> sorted_values = data.histogram_values;
                std::sort(sorted_values.begin(), sorted_values.end());
                
                for (double bucket : histogram_buckets_) {
                    uint64_t count = 0;
                    for (double value : sorted_values) {
                        if (value <= bucket) {
                            count++;
                        }
                    }
                    ss << name << "_bucket{le=\"" << bucket << "\"} " << count << "\n";
                }
                
                // Add +Inf bucket (total count)
                ss << name << "_bucket{le=\"+Inf\"} " << sorted_values.size() << "\n";
                
                // Add sum and count
                double sum = 0.0;
                for (double value : sorted_values) {
                    sum += value;
                }
                ss << name << "_sum " << sum << "\n";
                ss << name << "_count " << sorted_values.size() << "\n";
                break;
        }
        
        ss << "\n";
    }
    
    return ss.str();
}

std::string Metrics::export_json() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"metrics\": {\n";
    
    bool first = true;
    for (const auto& pair : metrics_) {
        if (!first) {
            ss << ",\n";
        }
        first = false;
        
        const std::string& name = pair.first;
        const MetricData& data = pair.second;
        
        ss << "    \"" << name << "\": {\n";
        ss << "      \"type\": ";
        
        switch (data.type) {
            case MetricType::COUNTER:
                ss << "\"counter\"";
                break;
            case MetricType::GAUGE:
                ss << "\"gauge\"";
                break;
            case MetricType::HISTOGRAM:
                ss << "\"histogram\"";
                break;
        }
        
        ss << ",\n";
        ss << "      \"value\": " << data.value << ",\n";
        ss << "      \"timestamp\": " << data.last_updated;
        
        if (data.type == MetricType::HISTOGRAM) {
            ss << ",\n";
            ss << "      \"samples\": " << data.histogram_values.size();
        }
        
        ss << "\n    }";
    }
    
    ss << "\n  }\n";
    ss << "}\n";
    
    return ss.str();
}

std::string Metrics::export_influxdb() const {
    // InfluxDB line protocol format
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    std::stringstream ss;
    
    for (const auto& pair : metrics_) {
        const std::string& name = pair.first;
        const MetricData& data = pair.second;
        
        ss << name << " value=" << data.value << " " << data.last_updated << "\n";
    }
    
    return ss.str();
}

void Metrics::set_default_labels(const std::unordered_map<std::string, std::string>& labels) {
    default_labels_ = labels;
}

void Metrics::set_histogram_buckets(const std::vector<double>& buckets) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    histogram_buckets_ = buckets;
}

void Metrics::set_max_histogram_samples(size_t max_samples) {
    max_histogram_samples_ = max_samples;
}

std::vector<Metrics::MetricSnapshot> Metrics::get_snapshot() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    std::vector<MetricSnapshot> snapshot;
    
    for (const auto& pair : metrics_) {
        MetricSnapshot snap;
        snap.name = pair.first;
        snap.labels = pair.second.labels;
        snap.timestamp_ms = pair.second.last_updated;
        
        switch (pair.second.type) {
            case MetricType::COUNTER:
                snap.type = "counter";
                snap.values["value"] = pair.second.value;
                break;
                
            case MetricType::GAUGE:
                snap.type = "gauge";
                snap.values["value"] = pair.second.value;
                break;
                
            case MetricType::HISTOGRAM:
                snap.type = "histogram";
                snap.values["count"] = static_cast<double>(pair.second.histogram_values.size());
                
                if (!pair.second.histogram_values.empty()) {
                    double sum = 0.0;
                    for (double value : pair.second.histogram_values) {
                        sum += value;
                    }
                    snap.values["sum"] = sum;
                    snap.values["mean"] = sum / pair.second.histogram_values.size();
                }
                break;
        }
        
        snapshot.push_back(snap);
    }
    
    return snapshot;
}

void Metrics::restore_snapshot(const std::vector<MetricSnapshot>& snapshot) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    for (const auto& snap : snapshot) {
        MetricType type;
        if (snap.type == "counter") {
            type = MetricType::COUNTER;
        } else if (snap.type == "gauge") {
            type = MetricType::GAUGE;
        } else if (snap.type == "histogram") {
            type = MetricType::HISTOGRAM;
        } else {
            continue;
        }
        
        MetricData data(type);
        data.labels = snap.labels;
        data.timestamp_ms = snap.timestamp_ms;
        
        auto value_it = snap.values.find("value");
        if (value_it != snap.values.end()) {
            data.value = value_it->second;
        }
        
        if (type == MetricType::HISTOGRAM) {
            // Histogram samples are not fully restored in this simplified version
            // In a real implementation, you might store and restore full sample data
        }
        
        metrics_[snap.name] = data;
    }
}

void Metrics::set_alert_threshold(const std::string& metric_name, double threshold, AlertCallback callback) {
    std::lock_guard<std::mutex> lock(alerts_mutex_);
    alert_thresholds_[metric_name] = std::make_pair(threshold, callback);
}

void Metrics::remove_alert_threshold(const std::string& metric_name) {
    std::lock_guard<std::mutex> lock(alerts_mutex_);
    alert_thresholds_.erase(metric_name);
}

Metrics::MetricsStats Metrics::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void Metrics::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = MetricsStats{};
    stats_.total_metrics = 0;
    stats_.total_samples = 0;
    stats_.memory_usage_bytes = 0;
    stats_.avg_update_time_us = 0.0;
}

void Metrics::update_metric(const std::string& name, MetricType type, double value,
                          const std::unordered_map<std::string, std::string>& labels) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it == metrics_.end()) {
        metrics_[name] = MetricData(type);
        it = metrics_.find(name);
    }
    
    if (it->second.type != type) {
        // Type mismatch, replace with new type
        metrics_[name] = MetricData(type);
        it = metrics_.find(name);
    }
    
    it->second.labels = labels;
    it->second.last_updated = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    switch (type) {
        case MetricType::COUNTER:
            it->second.value += value;
            break;
        case MetricType::GAUGE:
            it->second.value = value;
            break;
        case MetricType::HISTOGRAM:
            add_histogram_sample(it->second, value);
            break;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto update_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    update_stats(update_time.count());
    check_alerts(name, it->second.value);
}

void Metrics::check_alerts(const std::string& name, double value) {
    std::lock_guard<std::mutex> lock(alerts_mutex_);
    
    auto it = alert_thresholds_.find(name);
    if (it != alert_thresholds_.end()) {
        double threshold = it->second.first;
        const auto& callback = it->second.second;
        
        if (value > threshold) {
            // Call alert callback in a separate thread to avoid blocking
            std::thread(callback, name, value, threshold).detach();
        }
    }
}

std::string Metrics::format_labels(const std::unordered_map<std::string, std::string>& labels) const {
    if (labels.empty()) {
        return "";
    }
    
    std::stringstream ss;
    ss << "{";
    
    bool first = true;
    for (const auto& label : labels) {
        if (!first) {
            ss << ",";
        }
        first = false;
        
        ss << label.first << "=\"" << escape_prometheus_label(label.second) << "\"";
    }
    
    ss << "}";
    return ss.str();
}

std::string Metrics::escape_prometheus_label(const std::string& label) const {
    std::string escaped;
    for (char c : label) {
        if (c == '\\' || c == '"' || c == '\n') {
            escaped += '\\';
        }
        escaped += c;
    }
    return escaped;
}

void Metrics::add_histogram_sample(MetricData& data, double value) {
    data.histogram_values.push_back(value);
    
    // Limit the number of samples to prevent memory growth
    if (data.histogram_values.size() > max_histogram_samples_) {
        // Remove oldest samples (simple FIFO)
        data.histogram_values.erase(data.histogram_values.begin(), 
                                 data.histogram_values.begin() + (data.histogram_values.size() - max_histogram_samples_));
    }
}

void Metrics::update_stats(double update_time_us) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_metrics = metrics_.size();
    stats_.total_samples++;
    
    // Update average update time
    double total_time = stats_.avg_update_time_us * (stats_.total_samples - 1);
    stats_.avg_update_time_us = (total_time + update_time_us) / stats_.total_samples;
    
    // Estimate memory usage (simplified)
    stats_.memory_usage_bytes = estimate_memory_usage();
}

uint64_t Metrics::estimate_memory_usage() const {
    uint64_t usage = 0;
    
    for (const auto& pair : metrics_) {
        // Base metric data
        usage += sizeof(MetricData);
        
        // Histogram samples
        usage += pair.second.histogram_values.size() * sizeof(double);
        
        // Labels
        for (const auto& label : pair.second.labels) {
            usage += label.first.size() + label.second.size();
        }
    }
    
    return usage;
}

// Timer implementation
Metrics::Timer::Timer(const std::string& name) 
    : name_(name), stopped_(false) {
    start_time_ = std::chrono::high_resolution_clock::now();
}

Metrics::Timer::~Timer() {
    if (!stopped_) {
        stop();
    }
}

void Metrics::Timer::stop() {
    if (stopped_) {
        return;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_);
    
    Metrics::instance().record_histogram(name_ + "_duration_ms", duration.count());
    stopped_ = true;
}

double Metrics::Timer::elapsed_ms() const {
    auto current_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time_);
    return duration.count();
}

} // namespace dann
