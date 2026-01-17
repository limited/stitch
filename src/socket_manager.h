#ifndef SOCKET_MANAGER_H
#define SOCKET_MANAGER_H

#include <string>
#include <vector>
#include <cstdint>
#include <sys/epoll.h>

class SocketManager {
public:
    struct Event {
        int fd;
        bool readable;
        bool writable;
        bool error;
        bool hangup;
    };

    SocketManager();
    ~SocketManager();

    bool bind(const std::string& host, int port);
    bool listen(int backlog = 128);
    int acceptConnection();

    bool initEpoll();
    bool addToEpoll(int fd, uint32_t events);
    bool removeFromEpoll(int fd);
    bool modifyEpoll(int fd, uint32_t events);

    int waitForEvents(int timeout_ms = -1);
    std::vector<Event> getEvents() const;

    void close(int fd);
    void closeAll();

    int getListenFd() const;

private:
    int listen_fd_;
    int epoll_fd_;
    std::vector<struct epoll_event> events_;
    static constexpr int MAX_EVENTS = 64;

    void setNonBlocking(int fd);
};

#endif // SOCKET_MANAGER_H
