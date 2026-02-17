#include "stats.h"

Stats::Stats()
    : total_requests_(0), cache_hits_(0), start_time_(std::chrono::steady_clock::now()), total_latency_(0) {}

void Stats::recordRequest(std::chrono::steady_clock::duration duration, bool cache_hit) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++total_requests_;
    if (cache_hit) {
        ++cache_hits_;
    }
    total_latency_ += std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
}

double Stats::requestsPerSecond() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto elapsed = std::chrono::steady_clock::now() - start_time_;
    double seconds = std::chrono::duration_cast<std::chrono::duration<double>>(elapsed).count();
    if (seconds <= 0.0) {
        return 0.0;
    }
    return static_cast<double>(total_requests_) / seconds;
}

double Stats::averageResponseMs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (total_requests_ == 0) {
        return 0.0;
    }
    double avg_ns = static_cast<double>(total_latency_.count()) / static_cast<double>(total_requests_);
    return avg_ns / 1'000'000.0;
}

double Stats::cacheHitRate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (total_requests_ == 0) {
        return 0.0;
    }
    return static_cast<double>(cache_hits_) / static_cast<double>(total_requests_);
}

uint64_t Stats::totalRequests() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_requests_;
}
