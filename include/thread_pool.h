#pragma once

#include <pthread.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <vector>

class ThreadPool {
public:
    ThreadPool(size_t thread_count, std::function<void(int)> handler);
    ~ThreadPool();

    void enqueue(int client_fd);
    void shutdown();

private:
    static void* workerTrampoline(void* arg);
    void workerLoop();

    std::function<void(int)> handler_;
    std::queue<int> tasks_;
    pthread_mutex_t mutex_;
    pthread_cond_t cond_;
    std::vector<pthread_t> threads_;
    bool stopping_;
};
