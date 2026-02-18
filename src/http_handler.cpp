#include "http_handler.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

HttpHandler::HttpHandler(std::string root_dir, Cache* cache, Stats* stats)
    : root_dir_(std::move(root_dir)), cache_(cache), stats_(stats) {
    if (!root_dir_.empty() && root_dir_.back() == '/') {
        root_dir_.pop_back();
    }
}

void HttpHandler::handleClient(int client_fd) {
    auto start = std::chrono::steady_clock::now();
    bool cache_hit = false;

    std::string request;
    char buffer[4096];
    while (true) {
        ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            break;
        }
        request.append(buffer, n);
        if (request.find("\r\n\r\n") != std::string::npos || request.size() > 8192) {
            break;
        }
    }

    auto sendAll = [](int fd, const std::string& data) {
        size_t total = 0;
        while (total < data.size()) {
            ssize_t sent = send(fd, data.data() + total, data.size() - total, 0);
            if (sent < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            total += static_cast<size_t>(sent);
        }
    };

    auto sendAndClose = [&](const std::string& resp) {
        sendAll(client_fd, resp);
        close(client_fd);
    };

    if (request.empty() || request.size() > 8192) {
        sendAndClose(buildResponse(400, "text/plain", "Bad Request"));
        return;
    }

    size_t line_end = request.find("\r\n");
    if (line_end == std::string::npos) {
        sendAndClose(buildResponse(400, "text/plain", "Bad Request"));
        return;
    }

    std::istringstream iss(request.substr(0, line_end));
    std::string method, path, version;
    iss >> method >> path >> version;

    if (method.empty() || path.empty() || version.empty() || iss.fail()) {
        sendAndClose(buildResponse(400, "text/plain", "Bad Request"));
        return;
    }

    if (method != "GET") {
        sendAndClose(buildResponse(400, "text/plain", "Only GET supported"));
        return;
    }

    if (path.compare(0, 7, "http://") == 0 || path.compare(0, 8, "https://") == 0) {
        size_t scheme_end = path.find("//");
        size_t after_host = (scheme_end == std::string::npos) ? std::string::npos : path.find('/', scheme_end + 2);
        path = (after_host == std::string::npos) ? "/" : path.substr(after_host);
    }

    size_t query_pos = path.find('?');
    if (query_pos != std::string::npos) {
        path = path.substr(0, query_pos);
    }
    size_t frag_pos = path.find('#');
    if (frag_pos != std::string::npos) {
        path = path.substr(0, frag_pos);
    }

    if (path.empty() || path[0] != '/' || !isPathSafe(path)) {
        sendAndClose(buildResponse(400, "text/plain", "Bad Request"));
        return;
    }

    if (path == "/") {
        path = "/index.html";
    }

    std::string full_path = root_dir_ + path;
    std::string body;

    if (cache_ && cache_->get(full_path, body)) {
        cache_hit = true;
    } else {
        if (!readFile(full_path, body)) {
            sendAndClose(buildResponse(404, "text/plain", "Not Found"));
            if (stats_) {
                stats_->recordRequest(std::chrono::steady_clock::now() - start, false);
            }
            return;
        }
        if (cache_) {
            cache_->put(full_path, body);
        }
    }

    std::string response = buildResponse(200, guessContentType(full_path), body);
    sendAndClose(response);

    if (stats_) {
        stats_->recordRequest(std::chrono::steady_clock::now() - start, cache_hit);
    }
}

std::string HttpHandler::buildResponse(int status_code, const std::string& content_type, const std::string& body) {
    std::ostringstream oss;
    std::string status_text;
    switch (status_code) {
        case 200:
            status_text = "OK";
            break;
        case 400:
            status_text = "Bad Request";
            break;
        case 404:
            status_text = "Not Found";
            break;
        default:
            status_text = "Internal Server Error";
            break;
    }

    oss << "HTTP/1.1 " << status_code << ' ' << status_text << "\r\n";
    oss << "Content-Type: " << content_type << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;
    return oss.str();
}

std::string HttpHandler::guessContentType(const std::string& path) {
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".html") {
        return "text/html";
    }
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".css") {
        return "text/css";
    }
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".js") {
        return "application/javascript";
    }
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".png") {
        return "image/png";
    }
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".jpg") {
        return "image/jpeg";
    }
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".jpeg") {
        return "image/jpeg";
    }
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".gif") {
        return "image/gif";
    }
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".svg") {
        return "image/svg+xml";
    }
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".ico") {
        return "image/x-icon";
    }
    return "application/octet-stream";
}

bool HttpHandler::readFile(const std::string& path, std::string& out_content) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    out_content = ss.str();
    return true;
}

bool HttpHandler::isPathSafe(const std::string& path) {
    return path.find("..") == std::string::npos;
}
