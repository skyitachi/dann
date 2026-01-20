#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace dann {

class Metrics {
public:
    static Metrics& instance();
    
    // Counter metrics
    void increment_counter(const std::string& name, double value = 1.0);
    void decrement_counter(const std::string& name, double value = 1.0);
    void set_counter(const std::string& name, double value);
    double get_counter(const std::string& name) const;
    
    // Gauge metrics
    void set_gauge(const std::string& name, double value);
    double get_gauge(const std::string& name) const;
    
    // Histogram metrics
    void record_histogram(const std::string& name, double value);
    std::vector<double> get_histogram_values(const std::string& name) const;
    double get_histogram_percentile(const std::string& name, double percentile) const;
    double get_histogram_mean(const std::string& name) const;
    double get_histogram_sum(const std::string& name) const;
    uint64_t get_histogram_count(const std::string& name) const;
    
    // Timer metrics
    class Timer {
    public:
        Timer(const std::string& name);
        ~Timer();
        
        void stop();
        double elapsed_ms() const;
        
    private:
        std::string name_;
        std::chrono::high_resolution_clock::time_point start_time_;
        bool stopped_;
    };
    
    std::unique_ptr<Timer> start_timer(const std::string& name);
    
    // Metric families (for labeled metrics)
    void increment_counter_with_labels(const std::string& name, 
                                     const std::unordered_map<std::string, std::string>& labels,
                                     double value = 1.0);
    void set_gauge_with_labels(const std::string& name,
                              const std::unordered_map<std::string, std::string>& labels,
                              double value);
    void record_histogram_with_labels(const std::string& name,
                                     const std::unordered_map<std::string, std::string>& labels,
                                     double value);
    
    // Metric management
    void remove_metric(const std::string& name);
    void clear_all_metrics();
    std::vector<std::string> get_metric_names() const;
    
    // Export formats
    std::string export_prometheus() const;
    std::string export_json() const;
    std::string export_influxdb() const;
    
    // Configuration
    void set_default_labels(const std::unordered_map<std::string, std::string>& labels);
    void set_histogram_buckets(const std::vector<double>& buckets);
    void set_max_histogram_samples(size_t max_samples);
    
    // Aggregation
    struct MetricSnapshot {
        std::string name;
        std::string type;
        std::unordered_map<std::string, double> values;
        std::unordered_map<std::string, std::string> labels;
        uint64_t timestamp_ms;
    };
    
    std::vector<MetricSnapshot> get_snapshot() const;
    void restore_snapshot(const std::vector<MetricSnapshot>& snapshot);
    
    // Monitoring and alerting
    using AlertCallback = std::function<void(const std::string&, double, double)>;
    void set_alert_threshold(const std::string& metric_name, double threshold, AlertCallback callback);
    void remove_alert_threshold(const std::string& metric_name);
    
    // Statistics
    struct MetricsStats {
        uint64_t total_metrics;
        uint64_t total_samples;
        uint64_t memory_usage_bytes;
        double avg_update_time_us;
    };
    
    MetricsStats get_stats() const;
    void reset_stats();
    
private:
    Metrics();
    ~Metrics();
    
    // Prevent copying
    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;
    
    enum class MetricType {
        COUNTER,
        GAUGE,
        HISTOGRAM
    };
    
    struct MetricData {
        MetricType type;
        double value;
        std::vector<double> histogram_values;
        std::unordered_map<std::string, std::string> labels;
        uint64_t last_updated;
        uint64_t timestamp_ms;
        
        MetricData() : type(MetricType::COUNTER), value(0.0), last_updated(0), timestamp_ms(0) {}
        MetricData(MetricType t) : type(t), value(0.0), last_updated(0), timestamp_ms(0) {}
    };
    
    mutable std::mutex metrics_mutex_;
    std::unordered_map<std::string, MetricData> metrics_;
    std::unordered_map<std::string, std::string> default_labels_;
    std::vector<double> histogram_buckets_;
    size_t max_histogram_samples_;
    
    mutable std::mutex alerts_mutex_;
    std::unordered_map<std::string, std::pair<double, AlertCallback>> alert_thresholds_;
    
    mutable std::mutex stats_mutex_;
    MetricsStats stats_;
    
    // Internal methods
    void update_metric(const std::string& name, MetricType type, double value,
                     const std::unordered_map<std::string, std::string>& labels = {});
    void check_alerts(const std::string& name, double value);
    std::string format_labels(const std::unordered_map<std::string, std::string>& labels) const;
    std::string escape_prometheus_label(const std::string& label) const;
    
    // Histogram helpers
    void add_histogram_sample(MetricData& data, double value);
    std::vector<double> calculate_percentiles(const std::vector<double>& values) const;
    double calculate_percentile(const std::vector<double>& values, double percentile) const;
    
    // Statistics
    void update_stats(double update_time_us);
    uint64_t estimate_memory_usage() const;
};

// RAII timer helper
class ScopedTimer {
public:
    ScopedTimer(const std::string& name) : timer_(Metrics::instance().start_timer(name)) {}
    ~ScopedTimer() = default;
    
private:
    std::unique_ptr<Metrics::Timer> timer_;
};

// Convenience macros
#define METRIC_COUNTER_INC(name) Metrics::instance().increment_counter(name)
#define METRIC_COUNTER_ADD(name, value) Metrics::instance().increment_counter(name, value)
#define METRIC_GAUGE_SET(name, value) Metrics::instance().set_gauge(name, value)
#define METRIC_HISTOGRAM_RECORD(name, value) Metrics::instance().record_histogram(name, value)
#define METRIC_TIMER_SCOPE(name) ScopedTimer timer(name)

// Labeled metrics macros
#define METRIC_COUNTER_INC_LABELS(name, labels) Metrics::instance().increment_counter_with_labels(name, labels)
#define METRIC_GAUGE_SET_LABELS(name, labels, value) Metrics::instance().set_gauge_with_labels(name, labels, value)
#define METRIC_HISTOGRAM_RECORD_LABELS(name, labels, value) Metrics::instance().record_histogram_with_labels(name, labels, value)

} // namespace dann
