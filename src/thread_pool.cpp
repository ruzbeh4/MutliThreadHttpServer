#include "thread_pool.h"

#include <unistd.h>

#include <iostream>

ThreadPool::ThreadPool(size_t thread_count, std::function<void(int)> handler)
    : handler_(std::move(handler)), stopping_(false) {
    pthread_mutex_init(&mutex_, nullptr);
    pthread_cond_init(&cond_, nullptr);

    threads_.resize(thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
        int rc = pthread_create(&threads_[i], nullptr, ThreadPool::workerTrampoline, this);
        if (rc != 0) {
            std::cerr << "Failed to create thread " << i << ": " << rc << "\n";
        }
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
    pthread_mutex_destroy(&mutex_);
    pthread_cond_destroy(&cond_);
}

void ThreadPool::enqueue(int client_fd) {
    pthread_mutex_lock(&mutex_);
    if (stopping_) {
        pthread_mutex_unlock(&mutex_);
        close(client_fd);
        return;
    }
    tasks_.push(client_fd);
    pthread_cond_signal(&cond_);
    pthread_mutex_unlock(&mutex_);
}

void ThreadPool::shutdown() {
    pthread_mutex_lock(&mutex_);
    if (stopping_) {
        pthread_mutex_unlock(&mutex_);
        return;
    }
    stopping_ = true;
    pthread_cond_broadcast(&cond_);
    pthread_mutex_unlock(&mutex_);

    for (pthread_t& t : threads_) {
        if (t) {
            pthread_join(t, nullptr);
        }
    }
}

void* ThreadPool::workerTrampoline(void* arg) {
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    pool->workerLoop();
    return nullptr;
}

void ThreadPool::workerLoop() {
    while (true) {
        int client_fd = -1;

        pthread_mutex_lock(&mutex_);
        while (tasks_.empty() && !stopping_) {
            pthread_cond_wait(&cond_, &mutex_);
        }
        if (stopping_ && tasks_.empty()) {
            pthread_mutex_unlock(&mutex_);
            break;
        }
        client_fd = tasks_.front();
        tasks_.pop();
        pthread_mutex_unlock(&mutex_);

        if (client_fd != -1) {
            handler_(client_fd);
        }
    }
}
