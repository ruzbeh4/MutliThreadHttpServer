#include "cache.h"

#include <algorithm>

Cache::Cache(size_t max_bytes)
    : max_bytes_(max_bytes), current_bytes_(0), hits_(0), misses_(0) {}

bool Cache::get(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(key);
    if (it == map_.end()) {
        ++misses_;
        return false;
    }

    touch(key, it->second.second);
    value = it->second.first;
    ++hits_;
    return true;
}

void Cache::put(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(key);
    if (it != map_.end()) {
        current_bytes_ -= it->second.first.size();
        it->second.first = value;
        touch(key, it->second.second);
    } else {
        lru_.push_front(key);
        map_[key] = {value, lru_.begin()};
    }
    current_bytes_ += value.size();
    evictIfNeeded();
}

void Cache::touch(const std::string& key, std::list<std::string>::iterator it) {
    lru_.erase(it);
    lru_.push_front(key);
    map_[key].second = lru_.begin();
}

void Cache::evictIfNeeded() {
    while (current_bytes_ > max_bytes_ && !lru_.empty()) {
        std::string victim = lru_.back();
        lru_.pop_back();
        auto it = map_.find(victim);
        if (it != map_.end()) {
            current_bytes_ -= it->second.first.size();
            map_.erase(it);
        }
    }
}

double Cache::hitRate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t total = hits_ + misses_;
    if (total == 0) {
        return 0.0;
    }
    return static_cast<double>(hits_) / static_cast<double>(total);
}

size_t Cache::sizeBytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_bytes_;
}
