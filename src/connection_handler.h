#ifndef CONNECTION_HANDLER_H
#define CONNECTION_HANDLER_H

#include <chrono>
#include "http_parser.h"
#include "command_interpreter.h"
#include "response_generator.h"

enum class ConnectionState {
    READING_REQUEST,
    PROCESSING_COMMAND,
    SENDING_RESPONSE,
    WAITING,
    CLOSING,
    CLOSED
};

class ConnectionHandler {
public:
    ConnectionHandler(int socket_fd);
    ~ConnectionHandler();

    void onReadable();
    void onWritable();
    void onTimer();

    ConnectionState getState() const;
    bool shouldClose() const;
    int getFd() const;
    void closeConnection();

private:
    int socket_fd_;
    ConnectionState state_;

    HttpParser parser_;
    CommandInterpreter interpreter_;
    ResponseGenerator generator_;

    TestCommand current_command_;
    std::string response_data_;
    size_t bytes_sent_;

    std::chrono::steady_clock::time_point delay_start_;
    int delay_duration_ms_;

    void handleRequest();
    void sendResponse();
    void executeDelayedBehavior();
};

#endif // CONNECTION_HANDLER_H
