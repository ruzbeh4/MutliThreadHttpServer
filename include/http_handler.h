#pragma once

#include <string>

#include "cache.h"
#include "stats.h"

class HttpHandler {
public:
    HttpHandler(std::string root_dir, Cache* cache, Stats* stats);

    void handleClient(int client_fd);

private:
    std::string root_dir_;
    Cache* cache_;
    Stats* stats_;

    std::string buildResponse(int status_code, const std::string& content_type, const std::string& body);
    std::string guessContentType(const std::string& path);
    bool readFile(const std::string& path, std::string& out_content);
    bool isPathSafe(const std::string& path);
};
