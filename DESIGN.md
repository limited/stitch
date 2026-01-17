# Stitch Architecture & Design

## Overview

Stitch is a single-threaded, event-driven HTTP server designed for negative testing and fuzzing HTTP proxies. It implements HTTP parsing and response generation from scratch to allow intentionally non-compliant behavior.

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                         main.cpp                            │
│                    (Event Loop & CLI)                       │
└─────────────────────────────────────────────────────────────┘
                              │
                ┌─────────────┴──────────────┐
                │                            │
        ┌───────▼────────┐          ┌───────▼────────┐
        │ SocketManager  │          │ConnectionHandler│
        │  (epoll I/O)   │          │ (per-connection)│
        └────────────────┘          └────────┬────────┘
                                              │
                        ┌─────────────────────┼──────────────────┐
                        │                     │                  │
                  ┌─────▼──────┐    ┌────────▼────────┐  ┌──────▼──────┐
                  │ HttpParser │    │CommandInterpreter│  │  Response   │
                  │  (request) │    │ (query params)   │  │  Generator  │
                  └────────────┘    └──────────────────┘  └─────────────┘
```

## Module Descriptions

### 1. HttpParser (`http_parser.h/cpp`)

**Purpose:** Parse HTTP/1.1 requests from raw socket data without using external libraries.

**Key Features:**
- Incremental parsing (handles partial requests)
- Request line parsing (method, path, HTTP version)
- Header parsing (key-value pairs)
- Query parameter extraction with URL decoding
- State tracking (INCOMPLETE, COMPLETE, ERROR)

**Design Decisions:**
- Uses internal buffer to accumulate data across multiple `parse()` calls
- Separates parsing logic into discrete phases (request line, headers, query params)
- Returns `ParseResult` enum to indicate parsing state
- Stores parsed data in `HttpRequest` struct for easy access

**Example Flow:**
```cpp
HttpParser parser;

// First chunk
parser.parse("GET /test?foo=bar HTTP", 22);  // INCOMPLETE

// Second chunk
parser.parse("/1.1\r\nHost: example.com\r\n\r\n", 31);  // COMPLETE

// Access parsed data
const HttpRequest& req = parser.getRequest();
// req.method = "GET"
// req.path = "/test?foo=bar"
// req.query_params["foo"] = "bar"
```

**Implementation Details:**
- `findEndOfHeaders()`: Locates `\r\n\r\n` to detect complete request
- `parseRequestLine()`: Splits on whitespace, validates HTTP version
- `parseHeader()`: Splits on colon, trims whitespace
- `extractQueryParams()`: Parses after `?`, splits on `&` and `=`
- `urlDecode()`: Handles `%XX` encoding and `+` → space

---

### 2. CommandInterpreter (`command_interpreter.h/cpp`)

**Purpose:** Convert query parameters from HTTP requests into structured test commands.

**Key Features:**
- Maps query parameter strings to `BehaviorType` enum
- Extracts behavior-specific parameters (delays, error codes, etc.)
- Validates command parameters
- Provides human-readable descriptions of commands

**Supported Behaviors:**
```cpp
enum class BehaviorType {
    NORMAL,              // Standard HTTP 200 OK
    ERROR_RESPONSE,      // Custom status code/reason
    CLOSE_IMMEDIATELY,   // Close without sending response
    CLOSE_AFTER_HEADERS, // Close after headers sent
    CLOSE_AFTER_PARTIAL, // Close after N bytes
    SLOW_RESPONSE,       // Delay before response
    SLOW_HEADERS,        // Throttle header sending
    SLOW_BODY,           // Throttle body sending
    INVALID_STATUS_LINE, // Malformed status line
    INVALID_HEADERS,     // Malformed headers
    WRONG_CONTENT_LENGTH,// Incorrect Content-Length
    MALFORMED_CHUNKING,  // Bad chunked encoding
    TIMEOUT              // Accept but never respond
};
```

**Query Parameter Mapping:**
| Query Param | Behavior | Additional Params |
|-------------|----------|-------------------|
| `?behavior=error` | ERROR_RESPONSE | `code=502`, `reason=Bad%20Gateway` |
| `?behavior=close` | CLOSE_IMMEDIATELY | - |
| `?behavior=slow` | SLOW_RESPONSE | `delay=5000` (ms) |
| `?behavior=slow_body` | SLOW_BODY | `rate=100` (bytes/sec) |
| `?behavior=close_partial` | CLOSE_AFTER_PARTIAL | `bytes=100` |

**Design Decisions:**
- String-to-enum conversion via `parseBehavior()`
- Integer parsing with default values (handles invalid input gracefully)
- Immutable `TestCommand` struct passed to other components
- Validation separate from parsing for flexibility

**Example:**
```cpp
CommandInterpreter interp;
std::map<std::string, std::string> params;
params["behavior"] = "error";
params["code"] = "502";
params["reason"] = "Bad Gateway";

TestCommand cmd = interp.interpret(params);
// cmd.behavior = BehaviorType::ERROR_RESPONSE
// cmd.status_code = 502
// cmd.reason_phrase = "Bad Gateway"
```

---

### 3. ResponseGenerator (`response_generator.h/cpp`)

**Purpose:** Generate HTTP responses (both compliant and intentionally malformed).

**Key Features:**
- Creates proper HTTP/1.1 responses
- Supports custom status codes and reason phrases
- Can generate non-spec-compliant responses
- Serializes responses to strings for socket transmission

**Response Structure:**
```cpp
struct HttpResponse {
    int status_code;
    std::string reason_phrase;
    std::map<std::string, std::string> headers;
    std::string body;

    // Malformation flags
    bool malform_status_line;
    bool malform_headers;
    bool wrong_content_length;
    int wrong_content_length_value;
    bool malform_chunking;
};
```

**Design Decisions:**
- Separation of generation (`generate()`) and serialization (`serialize()`)
- Static factory methods for common response types
- Three-stage serialization: status line → headers → body
- Intentional malformation based on flags

**Serialization Process:**
1. **Status Line:** `HTTP/1.1 <code> <reason>\r\n`
   - Malformed: `INVALID STATUS LINE\r\n`
2. **Headers:** `<name>: <value>\r\n` for each header
   - Malformed: `InvalidHeaderWithoutColon\r\n`
   - Auto-adds `Content-Length` or `Transfer-Encoding`
3. **Separator:** `\r\n`
4. **Body:** Raw content
   - Malformed chunking: `INVALID_CHUNK_SIZE\r\n<data>\r\n`

**Example:**
```cpp
ResponseGenerator gen;

// Normal response
HttpResponse resp = ResponseGenerator::createOkResponse("Hello");
std::string serialized = gen.serialize(resp);
// "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello"

// Malformed response
TestCommand cmd;
cmd.behavior = BehaviorType::WRONG_CONTENT_LENGTH;
HttpResponse bad = gen.generate(cmd);
// bad.wrong_content_length = true
// bad.wrong_content_length_value = 9999
```

---

### 4. ConnectionHandler (`connection_handler.h/cpp`)

**Purpose:** Manage the complete lifecycle of a single HTTP connection.

**Key Features:**
- Per-connection state machine
- Integrates parser, interpreter, and generator
- Handles all behavior types (delays, closes, malformed responses)
- Non-blocking I/O operations

**State Machine:**
```
READING_REQUEST → PROCESSING_COMMAND → SENDING_RESPONSE → CLOSING
                        ↓
                   WAITING (for delays)
```

**States:**
- `READING_REQUEST`: Accumulating request data, feeding to parser
- `PROCESSING_COMMAND`: Interpreting command, generating response
- `WAITING`: Delayed response (slow/timeout behaviors)
- `SENDING_RESPONSE`: Writing response data to socket
- `CLOSING`/`CLOSED`: Connection termination

**Event Handlers:**
- `onReadable()`: Called when socket has data to read
  - Reads from socket into buffer
  - Feeds data to HttpParser
  - Transitions to PROCESSING_COMMAND when complete

- `onWritable()`: Called when socket ready for writing
  - Sends response data
  - Handles partial writes (EAGAIN)
  - Supports slow sending behaviors

- `onTimer()`: Called periodically for time-based behaviors
  - Checks if delay has elapsed
  - Transitions from WAITING to SENDING_RESPONSE

**Behavior Implementation:**
- `CLOSE_IMMEDIATELY`: Set state to CLOSING in `handleRequest()`
- `CLOSE_AFTER_HEADERS`: Truncate response after `\r\n\r\n`
- `CLOSE_AFTER_PARTIAL`: Truncate response to N bytes
- `SLOW_RESPONSE`: Set delay timer, enter WAITING state
- `SLOW_BODY`: Limit bytes per send() call
- `TIMEOUT`: Enter WAITING with INT_MAX delay

**Design Decisions:**
- Socket FD ownership (closes in destructor)
- Response data buffered as string for simplicity
- Byte counter tracks partial sends
- Timer-based delays using `chrono::steady_clock`

**Example Flow:**
```cpp
// Connection accepted
ConnectionHandler handler(client_fd);

// Data arrives
handler.onReadable();  // Reads "GET /?behavior=error&code=502 HTTP/1.1\r\n\r\n"
                       // Parses, interprets, generates response
                       // State: SENDING_RESPONSE

// Socket writable
handler.onWritable();  // Sends "HTTP/1.1 502 Bad Gateway\r\n..."
                       // State: CLOSING

// Check if done
if (handler.shouldClose()) {
    handler.closeConnection();
}
```

---

### 5. SocketManager (`socket_manager.h/cpp`)

**Purpose:** Manage TCP sockets and epoll-based event loop.

**Key Features:**
- TCP socket creation and binding
- Non-blocking I/O configuration
- epoll integration (edge-triggered mode)
- Connection acceptance
- Event polling

**Design Decisions:**
- Uses Linux `epoll` for scalable event notification
- Edge-triggered mode (`EPOLLET`) for efficiency
- Non-blocking sockets for all file descriptors
- Separate methods for each socket operation

**API Methods:**

**Setup:**
```cpp
SocketManager mgr;
mgr.bind("0.0.0.0", 8080);        // Create and bind socket
mgr.listen();                      // Start listening
mgr.initEpoll();                   // Create epoll instance
```

**Event Loop:**
```cpp
int n = mgr.waitForEvents(100);    // Wait up to 100ms
if (n > 0) {
    int client_fd = mgr.acceptConnection();
    if (client_fd >= 0) {
        mgr.addToEpoll(client_fd, EPOLLIN | EPOLLOUT);
    }
}
```

**Cleanup:**
```cpp
mgr.removeFromEpoll(fd);
mgr.close(fd);
mgr.closeAll();  // Close listen socket and epoll
```

**Implementation Details:**
- `setNonBlocking()`: Uses `fcntl()` to set `O_NONBLOCK`
- `bind()`: Creates socket, sets `SO_REUSEADDR`, binds to address
- `acceptConnection()`: Accepts new connection, sets non-blocking
- `initEpoll()`: Creates epoll, adds listen socket
- `waitForEvents()`: Calls `epoll_wait()`, returns event count

**Edge-Triggered Mode:**
- Events triggered only on state changes (not level)
- Must read/write until EAGAIN to avoid missing events
- More efficient but requires careful handling

---

### 6. Main Server (`main.cpp`)

**Purpose:** Application entry point, CLI argument parsing, event loop orchestration.

**Key Features:**
- Command-line argument parsing
- Signal handling (graceful shutdown)
- Main event loop
- Connection management
- Integration of all components

**Main Event Loop:**
```cpp
while (running) {
    // Wait for events (100ms timeout)
    int n_events = socket_mgr.waitForEvents(100);

    // Accept new connections
    if (n_events > 0) {
        int client_fd = socket_mgr.acceptConnection();
        while (client_fd >= 0) {
            auto handler = std::make_unique<ConnectionHandler>(client_fd);
            connections[client_fd] = std::move(handler);
            socket_mgr.addToEpoll(client_fd, EPOLLIN | EPOLLOUT);
            client_fd = socket_mgr.acceptConnection();
        }
    }

    // Process existing connections
    for (auto& [fd, handler] : connections) {
        handler->onReadable();
        handler->onWritable();
        handler->onTimer();

        if (handler->shouldClose()) {
            socket_mgr.removeFromEpoll(fd);
            handler->closeConnection();
            to_remove.push_back(fd);
        }
    }

    // Clean up closed connections
    for (int fd : to_remove) {
        connections.erase(fd);
    }
}
```

**Signal Handling:**
- `SIGINT` and `SIGTERM` set `running = false`
- Graceful shutdown: closes all connections, then listen socket
- No abrupt termination

**Connection Management:**
- `std::map<int, std::unique_ptr<ConnectionHandler>>` tracks active connections
- Unique pointers ensure automatic cleanup
- File descriptor used as map key

**Design Decisions:**
- Single-threaded for simplicity (no locking needed)
- 100ms timeout prevents CPU spinning
- Calls all three event handlers (readable/writable/timer) each iteration
  - Simplified but may be inefficient for many connections
  - Good enough for testing tool use case
- Verbose mode logs connection lifecycle

---

## Data Flow

### Normal Request:
```
1. Client connects
   └→ SocketManager::acceptConnection()

2. ConnectionHandler created
   └→ Added to epoll with EPOLLIN | EPOLLOUT

3. Request arrives
   └→ ConnectionHandler::onReadable()
      └→ HttpParser::parse()
         └→ Returns COMPLETE

4. Request processed
   └→ CommandInterpreter::interpret()
   └→ ResponseGenerator::generate()
   └→ ResponseGenerator::serialize()

5. Response sent
   └→ ConnectionHandler::onWritable()
      └→ send() system call

6. Connection closed
   └→ ConnectionHandler::closeConnection()
   └→ Removed from connections map
```

### Error Response Example:
```
GET /?behavior=error&code=502&reason=Bad%20Gateway HTTP/1.1
   ↓
HttpParser extracts query_params["behavior"] = "error"
   ↓
CommandInterpreter creates TestCommand{
    behavior: ERROR_RESPONSE,
    status_code: 502,
    reason_phrase: "Bad Gateway"
}
   ↓
ResponseGenerator creates HttpResponse{
    status_code: 502,
    reason_phrase: "Bad Gateway",
    body: "Bad Gateway"
}
   ↓
serialize() produces:
"HTTP/1.1 502 Bad Gateway\r\nContent-Length: 11\r\n\r\nBad Gateway"
   ↓
ConnectionHandler sends to socket
```

## Thread Safety

**Current Design:** Single-threaded, no synchronization needed.

**Scalability Considerations:**
- Event loop handles multiple connections
- Epoll scales to thousands of connections
- For high load, could use thread pool pattern:
  - Main thread: epoll, accept connections
  - Worker threads: process requests
  - Would require mutex for connection map

## Performance Characteristics

**Memory:**
- O(N) where N = number of active connections
- Each connection ~1KB (buffers, state)
- HttpParser buffer grows with request size

**CPU:**
- O(N) per event loop iteration (checks all connections)
- Could optimize with proper epoll event handling
- Parser is O(M) where M = request size

**Latency:**
- Min latency: 100ms (event loop timeout)
- Could reduce timeout for lower latency
- Non-blocking I/O prevents head-of-line blocking

## Testing Strategy

**Unit Tests:**
- HttpParser: 17 tests (incremental parsing, edge cases)
- CommandInterpreter: 20 tests (all behavior types, validation)
- ResponseGenerator: 17 tests (serialization, malformation)
- Total: 54 tests, 100% pass rate

**Integration Tests:**
- Manual testing with curl
- Tests each behavior type end-to-end
- Verifies network layer works correctly

**Memory Safety:**
- Valgrind: 0 leaks, 0 errors
- 1,787 allocations, all freed

**Static Analysis:**
- Cppcheck: 0 issues
- 18+ enhanced compiler warnings
- Zero warnings with -Wall -Wextra -Wpedantic

## Build System

**CMake Structure:**
- `stitch_lib`: Static library with all core components
- `stitch`: Main executable (links stitch_lib)
- `run_tests`: Test executable (links stitch_lib + CppUnit)

**Targets:**
- `make`: Build all
- `make test`: Run unit tests
- `make cppcheck`: Static analysis
- `make valgrind`: Memory check
- `make analyze`: All checks

## Future Enhancements

**Possible Improvements:**
1. **Proper Event Handling:** Track which FDs have events instead of polling all
2. **Timer Infrastructure:** Use timerfd for accurate delays
3. **HTTP/2 Support:** Extend to test HTTP/2 edge cases
4. **SSL/TLS:** Add OpenSSL for HTTPS testing
5. **Configurable Behaviors:** YAML config file for complex scenarios
6. **Metrics:** Prometheus endpoint for observability
7. **Threading:** Thread pool for CPU-bound operations

**Current Limitations:**
- Linux-only (epoll)
- HTTP/1.1 only
- No persistent connections (Connection: close)
- Simplified slow sending (not truly rate-limited)
