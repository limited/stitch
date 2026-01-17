#include "socket_manager.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>

SocketManager::SocketManager()
    : listen_fd_(-1)
    , epoll_fd_(-1)
    , events_(MAX_EVENTS) {
}

SocketManager::~SocketManager() {
    closeAll();
}

bool SocketManager::bind(const std::string& host, int port) {
    // Create socket
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        return false;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    // Set up address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (host == "0.0.0.0" || host.empty()) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }
    }

    // Bind socket
    if (::bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    // Set non-blocking
    setNonBlocking(listen_fd_);

    return true;
}

bool SocketManager::listen(int backlog) {
    if (listen_fd_ < 0) {
        return false;
    }

    if (::listen(listen_fd_, backlog) < 0) {
        return false;
    }

    return true;
}

int SocketManager::acceptConnection() {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        return -1;
    }

    // Set non-blocking
    setNonBlocking(client_fd);

    return client_fd;
}

bool SocketManager::initEpoll() {
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        return false;
    }

    // Add listen socket to epoll
    return addToEpoll(listen_fd_, EPOLLIN);
}

bool SocketManager::addToEpoll(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events | EPOLLET;  // Edge-triggered mode
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        return false;
    }

    return true;
}

bool SocketManager::removeFromEpoll(int fd) {
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        return false;
    }

    return true;
}

bool SocketManager::modifyEpoll(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events | EPOLLET;  // Edge-triggered mode
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        return false;
    }

    return true;
}

int SocketManager::waitForEvents(int timeout_ms) {
    int n = epoll_wait(epoll_fd_, events_.data(), MAX_EVENTS, timeout_ms);
    return n;
}

std::vector<SocketManager::Event> SocketManager::getEvents() const {
    std::vector<Event> result;
    // This should be called after waitForEvents() returns
    // For simplicity, we'll return an empty vector here
    // The actual events should be processed in waitForEvents()
    return result;
}

void SocketManager::close(int fd) {
    if (fd >= 0) {
        ::close(fd);
    }
}

void SocketManager::closeAll() {
    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

int SocketManager::getListenFd() const {
    return listen_fd_;
}

void SocketManager::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return;
    }

    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
