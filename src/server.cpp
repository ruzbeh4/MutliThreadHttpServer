#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

Server::Server(int port, size_t /*thread_count*/, const std::string& root_dir, ThreadPool* pool)
    : port_(port), listen_fd_(-1), running_(false), pool_(pool), root_dir_(root_dir) {}

Server::~Server() { stop(); }

bool Server::setupSocket() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        std::perror("socket");
        return false;
    }

    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::perror("setsockopt");
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("bind");
        return false;
    }

    if (listen(listen_fd_, SOMAXCONN) < 0) {
        std::perror("listen");
        return false;
    }

    return true;
}

bool Server::start() {
    if (!setupSocket()) {
        return false;
    }

    running_.store(true);
    std::cout << "Server listening on port " << port_ << " (root: " << root_dir_ << ")" << std::endl;

    while (running_.load()) {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (!running_.load()) {
                break;
            }
            std::perror("accept");
            continue;
        }

        pool_->enqueue(client_fd);
    }

    if (listen_fd_ != -1) {
        close(listen_fd_);
        listen_fd_ = -1;
    }
    return true;
}

void Server::stop() {
    running_.store(false);
    if (listen_fd_ != -1) {
        shutdown(listen_fd_, SHUT_RDWR);
        close(listen_fd_);
        listen_fd_ = -1;
    }
    if (pool_) {
        pool_->shutdown();
    }
}
