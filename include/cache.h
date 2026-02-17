#pragma once

#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

class Cache {
public:
    explicit Cache(size_t max_bytes);

    bool get(const std::string& key, std::string& value);
    void put(const std::string& key, const std::string& value);

    double hitRate() const;
    size_t sizeBytes() const;

private:
    mutable std::mutex mutex_;
    size_t max_bytes_;
    size_t current_bytes_;
    size_t hits_;
    size_t misses_;

    std::list<std::string> lru_;
    std::unordered_map<std::string, std::pair<std::string, std::list<std::string>::iterator>> map_;

    void touch(const std::string& key, std::list<std::string>::iterator it);
    void evictIfNeeded();
};
