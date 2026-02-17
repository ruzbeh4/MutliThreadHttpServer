#pragma once

#include <atomic>
#include <string>

#include "thread_pool.h"

class Server {
public:
    Server(int port, size_t thread_count, const std::string& root_dir, ThreadPool* pool);
    ~Server();

    bool start();
    void stop();

private:
    int port_;
    int listen_fd_;
    std::atomic<bool> running_;
    ThreadPool* pool_;
    std::string root_dir_;

    bool setupSocket();
};
