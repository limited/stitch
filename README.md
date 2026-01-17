# Stitch - HTTP Negative Testing Utility

[![CI](https://github.com/limited/stitch/actions/workflows/ci.yml/badge.svg)](https://github.com/limited/stitch/actions/workflows/ci.yml)

Stitch is a C++ HTTP server testing utility designed for negative testing and fuzzing HTTP proxies. It simulates various edge cases, malformed responses, and error conditions that can help test the robustness of HTTP clients and proxies.

## Documentation

- **[USAGE.md](USAGE.md)** - Complete usage guide with all command-line options and query parameters
- **[DESIGN.md](DESIGN.md)** - Architecture and implementation details

## Features

- **Custom HTTP Implementation**: Implements HTTP parsing and response generation from scratch (no frameworks)
- **Non-Spec-Compliant Responses**: Can intentionally send malformed HTTP responses
- **Query Parameter Control**: Behaviors controlled via URL query parameters
- **Single-Threaded Event Loop**: Uses epoll for efficient connection handling
- **Multiple Test Scenarios**:
  - Custom error codes and reason phrases
  - Connection closes at various stages
  - Slow response delivery
  - Malformed headers and chunked encoding
  - Invalid Content-Length
  - And more...

## Building

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+)
- CMake 3.14 or later
- CppUnit (for tests)

### Build Instructions

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
make

# Run tests
make test
# or directly:
./tests/run_tests

# Install (optional)
sudo make install
```

## Usage

Start the server:

```bash
./stitch -p 8080 -h 0.0.0.0
```

Options:
- `-p, --port <port>`: Port to listen on (default: 8080)
- `-h, --host <host>`: Host to bind to (default: 0.0.0.0)
- `-v, --verbose`: Enable verbose logging

## Query Parameter API

Control server behavior via query parameters in your HTTP requests:

### Normal Response
```bash
curl "http://localhost:8080/"
# Returns HTTP 200 OK with default body
```

### Error Response with Custom Code
```bash
curl "http://localhost:8080/?behavior=error&code=502&reason=Bad%20Gateway"
# Returns HTTP 502 with custom reason phrase
```

### Close Connection
```bash
# Close immediately without response
curl "http://localhost:8080/?behavior=close"

# Close after sending headers
curl "http://localhost:8080/?behavior=close_headers"

# Close after partial body
curl "http://localhost:8080/?behavior=close_partial&bytes=100"
```

### Slow Response
```bash
# Delay entire response by 5 seconds
curl "http://localhost:8080/?behavior=slow&delay=5000"

# Send body at 100 bytes/second
curl "http://localhost:8080/?behavior=slow_body&rate=100"
```

### Malformed Responses
```bash
# Invalid status line
curl "http://localhost:8080/?behavior=invalid_status"

# Malformed headers
curl "http://localhost:8080/?behavior=invalid_headers"

# Wrong Content-Length
curl "http://localhost:8080/?behavior=wrong_length&length=9999"

# Malformed chunked encoding
curl "http://localhost:8080/?behavior=malformed_chunking"
```

### Timeout (No Response)
```bash
curl "http://localhost:8080/?behavior=timeout"
# Server accepts connection but never sends response
```

## Architecture

Stitch is composed of several modular components:

- **HttpParser**: Parses incoming HTTP requests
- **CommandInterpreter**: Converts query parameters into test commands
- **ResponseGenerator**: Generates compliant and non-compliant responses
- **ConnectionHandler**: Manages per-connection state machine
- **SocketManager**: Handles epoll event loop and socket I/O

All components are unit-tested using CppUnit with 100% test coverage.

## Development

The project follows Test-Driven Development (TDD) practices:

1. Tests are written first
2. Implementation follows to pass tests
3. All tests must pass at 100% before moving forward

Run tests:
```bash
cd build
./tests/run_tests
```

## Security Note

This tool is intended for authorized security testing, defensive security research, CTF challenges, and educational contexts. It should only be used in controlled environments for testing purposes.

## License

This project is provided for educational and testing purposes.
