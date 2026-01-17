#include <iostream>
#include <map>
#include <vector>
#include <csignal>
#include <cstring>
#include <memory>
#include "socket_manager.h"
#include "connection_handler.h"

// Global flag for graceful shutdown
static volatile bool running = true;

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
        running = false;
    }
}

void printUsage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n"
              << "Options:\n"
              << "  -p, --port <port>     Port to listen on (default: 8080)\n"
              << "  -h, --host <host>     Host to bind to (default: 0.0.0.0)\n"
              << "  -v, --verbose         Enable verbose logging\n"
              << "  --help                Show this help message\n";
}

int main(int argc, char* argv[]) {
    std::string host = "0.0.0.0";
    int port = 8080;
    bool verbose = false;

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                port = std::atoi(argv[++i]);
            } else {
                std::cerr << "Error: " << arg << " requires an argument\n";
                return 1;
            }
        } else if (arg == "-h" || arg == "--host") {
            if (i + 1 < argc) {
                host = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires an argument\n";
                return 1;
            }
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "Stitch HTTP Negative Testing Utility\n";
    std::cout << "Starting server on " << host << ":" << port << "\n";

    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Create socket manager
    SocketManager socket_mgr;

    // Bind and listen
    if (!socket_mgr.bind(host, port)) {
        std::cerr << "Failed to bind to " << host << ":" << port << "\n";
        return 1;
    }

    if (!socket_mgr.listen()) {
        std::cerr << "Failed to listen on socket\n";
        return 1;
    }

    // Initialize epoll
    if (!socket_mgr.initEpoll()) {
        std::cerr << "Failed to initialize epoll\n";
        return 1;
    }

    std::cout << "Server listening on " << host << ":" << port << "\n";
    std::cout << "Press Ctrl+C to stop\n\n";

    // Map of file descriptors to connection handlers
    std::map<int, std::unique_ptr<ConnectionHandler>> connections;

    // Main event loop
    while (running) {
        // Wait for events (100ms timeout to check running flag)
        int n_events = socket_mgr.waitForEvents(100);

        if (n_events < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, continue
                continue;
            }
            std::cerr << "epoll_wait error: " << strerror(errno) << "\n";
            break;
        }

        // Process events
        // Note: We need to manually access epoll events since our API is simplified
        // In a real implementation, we'd improve the SocketManager API

        // For now, let's check if the listen socket is ready
        if (n_events > 0) {
            // Try to accept new connections
            int client_fd = socket_mgr.acceptConnection();
            while (client_fd >= 0) {
                if (verbose) {
                    std::cout << "Accepted new connection: fd=" << client_fd << "\n";
                }

                // Create connection handler
                auto handler = std::make_unique<ConnectionHandler>(client_fd);
                connections[client_fd] = std::move(handler);

                // Add to epoll
                socket_mgr.addToEpoll(client_fd, EPOLLIN | EPOLLOUT | EPOLLET);

                // Try to accept more connections
                client_fd = socket_mgr.acceptConnection();
            }
        }

        // Process existing connections
        std::vector<int> to_remove;

        for (auto& pair : connections) {
            int fd = pair.first;
            auto& handler = pair.second;

            // Call onReadable to try reading data
            handler->onReadable();

            // Call onWritable to try sending data
            handler->onWritable();

            // Call onTimer for delayed behaviors
            handler->onTimer();

            // Check if connection should be closed
            if (handler->shouldClose()) {
                if (verbose) {
                    std::cout << "Closing connection: fd=" << fd << "\n";
                }
                socket_mgr.removeFromEpoll(fd);
                handler->closeConnection();
                to_remove.push_back(fd);
            }
        }

        // Remove closed connections
        for (int fd : to_remove) {
            connections.erase(fd);
        }
    }

    std::cout << "Shutting down server...\n";

    // Clean up all connections
    for (auto& pair : connections) {
        socket_mgr.removeFromEpoll(pair.first);
        pair.second->closeConnection();
    }
    connections.clear();

    socket_mgr.closeAll();

    std::cout << "Server stopped.\n";
    return 0;
}
