#pragma once

#include <chrono>
#include <mutex>
#include <stdint.h>

class Stats {
public:
    Stats();

    void recordRequest(std::chrono::steady_clock::duration duration, bool cache_hit);

    double requestsPerSecond() const;
    double averageResponseMs() const;
    double cacheHitRate() const;
    uint64_t totalRequests() const;

private:
    mutable std::mutex mutex_;
    uint64_t total_requests_;
    uint64_t cache_hits_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::nanoseconds total_latency_;
};
