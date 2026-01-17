#include "connection_handler.h"
#include <unistd.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstring>

ConnectionHandler::ConnectionHandler(int socket_fd)
    : socket_fd_(socket_fd)
    , state_(ConnectionState::READING_REQUEST)
    , bytes_sent_(0)
    , delay_duration_ms_(0) {
}

ConnectionHandler::~ConnectionHandler() {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
    }
}

void ConnectionHandler::onReadable() {
    if (state_ != ConnectionState::READING_REQUEST) {
        return;
    }

    char buffer[4096];
    ssize_t n = recv(socket_fd_, buffer, sizeof(buffer), 0);

    if (n < 0) {
        if (errno == EAGAIN) {
            // No more data available right now (EAGAIN == EWOULDBLOCK on Linux)
            return;
        }
        // Error occurred
        state_ = ConnectionState::CLOSING;
        return;
    }

    if (n == 0) {
        // Connection closed by peer
        state_ = ConnectionState::CLOSING;
        return;
    }

    // Feed data to parser
    HttpParser::ParseResult result = parser_.parse(buffer, static_cast<size_t>(n));

    if (result == HttpParser::ParseResult::COMPLETE) {
        // Request fully parsed, process it
        state_ = ConnectionState::PROCESSING_COMMAND;
        handleRequest();
    } else if (result == HttpParser::ParseResult::ERROR) {
        // Parse error, close connection
        state_ = ConnectionState::CLOSING;
    }
    // If INCOMPLETE, wait for more data
}

void ConnectionHandler::onWritable() {
    if (state_ != ConnectionState::SENDING_RESPONSE) {
        return;
    }

    sendResponse();
}

void ConnectionHandler::onTimer() {
    if (state_ == ConnectionState::WAITING) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - delay_start_).count();

        if (elapsed >= delay_duration_ms_) {
            // Delay complete, send response
            state_ = ConnectionState::SENDING_RESPONSE;
            sendResponse();
        }
    }
}

ConnectionState ConnectionHandler::getState() const {
    return state_;
}

bool ConnectionHandler::shouldClose() const {
    return state_ == ConnectionState::CLOSED || state_ == ConnectionState::CLOSING;
}

int ConnectionHandler::getFd() const {
    return socket_fd_;
}

void ConnectionHandler::handleRequest() {
    const HttpRequest& request = parser_.getRequest();

    // Interpret command from query parameters
    current_command_ = interpreter_.interpret(request.query_params);

    // Handle special behaviors that don't require a response
    switch (current_command_.behavior) {
        case BehaviorType::CLOSE_IMMEDIATELY:
            state_ = ConnectionState::CLOSING;
            return;

        case BehaviorType::TIMEOUT:
            // Just wait forever (or until connection is closed)
            state_ = ConnectionState::WAITING;
            delay_duration_ms_ = INT32_MAX;
            delay_start_ = std::chrono::steady_clock::now();
            return;

        case BehaviorType::SLOW_RESPONSE:
            // Delay before sending response
            if (current_command_.delay_ms > 0) {
                state_ = ConnectionState::WAITING;
                delay_duration_ms_ = current_command_.delay_ms;
                delay_start_ = std::chrono::steady_clock::now();
                return;
            }
            break;

        default:
            break;
    }

    // Generate response
    HttpResponse response = generator_.generate(current_command_);
    response_data_ = generator_.serialize(response);
    bytes_sent_ = 0;

    // Check for close-after-headers behavior
    if (current_command_.behavior == BehaviorType::CLOSE_AFTER_HEADERS) {
        // Find end of headers
        size_t headers_end = response_data_.find("\r\n\r\n");
        if (headers_end != std::string::npos) {
            // Truncate to just headers
            response_data_.resize(headers_end + 4);
        }
    }

    // Check for partial close behavior
    if (current_command_.behavior == BehaviorType::CLOSE_AFTER_PARTIAL) {
        if (response_data_.length() > current_command_.bytes_before_close) {
            response_data_.resize(current_command_.bytes_before_close);
        }
    }

    // Start sending response
    state_ = ConnectionState::SENDING_RESPONSE;
    sendResponse();
}

void ConnectionHandler::sendResponse() {
    while (bytes_sent_ < response_data_.length()) {
        size_t remaining = response_data_.length() - bytes_sent_;
        size_t to_send = remaining;

        // Handle slow send behaviors
        if (current_command_.behavior == BehaviorType::SLOW_HEADERS ||
            current_command_.behavior == BehaviorType::SLOW_BODY) {
            if (current_command_.bytes_per_second > 0) {
                // Limit send size for slow behavior
                // (This is simplified; real implementation would use timers)
                to_send = std::min(remaining, size_t(current_command_.bytes_per_second / 10));
                if (to_send == 0) to_send = 1;
            }
        }

        ssize_t n = send(socket_fd_, response_data_.data() + bytes_sent_, to_send, 0);

        if (n < 0) {
            if (errno == EAGAIN) {
                // Socket buffer full, wait for next writable event (EAGAIN == EWOULDBLOCK on Linux)
                return;
            }
            // Error occurred
            state_ = ConnectionState::CLOSING;
            return;
        }

        bytes_sent_ += static_cast<size_t>(n);

        // For slow behaviors, break after each send to simulate slow sending
        if ((current_command_.behavior == BehaviorType::SLOW_HEADERS ||
             current_command_.behavior == BehaviorType::SLOW_BODY) &&
            current_command_.bytes_per_second > 0) {
            return;  // Wait for next onWritable() or timer event
        }
    }

    // All data sent, close connection
    state_ = ConnectionState::CLOSING;
}

void ConnectionHandler::executeDelayedBehavior() {
    // Handled by onTimer()
}

void ConnectionHandler::closeConnection() {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
    state_ = ConnectionState::CLOSED;
}
