#include <signal.h>
#include <unistd.h>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>

#include "cache.h"
#include "http_handler.h"
#include "server.h"
#include "stats.h"
#include "thread_pool.h"

static Server* g_server = nullptr;

void handleSignal(int) {
    if (g_server) {
        std::cout << "\nShutting down..." << std::endl;
        g_server->stop();
    }
}

struct Config {
    int port = 8080;
    size_t threads = 8;
    std::string root = "www";
    size_t cache_bytes = 5 * 1024 * 1024;  // 5 MB
    bool use_cache = true;
};

Config parseArgs(int argc, char* argv[]) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-p" && i + 1 < argc) {
            cfg.port = std::atoi(argv[++i]);
        } else if (arg == "-t" && i + 1 < argc) {
            cfg.threads = static_cast<size_t>(std::atoi(argv[++i]));
        } else if (arg == "-r" && i + 1 < argc) {
            cfg.root = argv[++i];
        } else if (arg == "-c" && i + 1 < argc) {
            cfg.cache_bytes = static_cast<size_t>(std::atoll(argv[++i]));
        } else if (arg == "--noLRU" || arg == "--no-cache") {
            cfg.use_cache = false;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: webserver [-p port] [-t threads] [-r root_dir] [-c cache_bytes] [--noLRU]\n";
            std::exit(0);
        }
    }
    return cfg;
}

int main(int argc, char* argv[]) {
    Config cfg = parseArgs(argc, argv);

    Cache cache(cfg.cache_bytes);
    Stats stats;
    Cache* cache_ptr = cfg.use_cache ? &cache : nullptr;
    HttpHandler handler(cfg.root, cache_ptr, &stats);

    ThreadPool pool(cfg.threads, [&](int fd) { handler.handleClient(fd); });
    Server server(cfg.port, cfg.threads, cfg.root, &pool);
    g_server = &server;

    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    if (!server.start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "Total requests handled: " << stats.totalRequests() << std::endl;
    std::cout << "Average response time (ms): " << stats.averageResponseMs() << std::endl;
    std::cout << "Cache hit rate: " << stats.cacheHitRate() * 100.0 << "%" << std::endl;
    std::cout << "Requests per second: " << stats.requestsPerSecond() << std::endl;

    return 0;
}
