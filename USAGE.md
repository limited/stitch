# Stitch Usage Guide

Complete guide to using Stitch for HTTP negative testing and proxy fuzzing.

## Table of Contents

- [Quick Start](#quick-start)
- [Command Line Options](#command-line-options)
- [Query Parameter API](#query-parameter-api)
- [Behavior Types](#behavior-types)
- [Usage Examples](#usage-examples)
- [Testing Scenarios](#testing-scenarios)
- [Troubleshooting](#troubleshooting)

---

## Quick Start

### Build and Run

```bash
# Build
mkdir build && cd build
cmake ..
make

# Run server on default port (8080)
./stitch

# Run server on custom port with verbose logging
./stitch -p 9000 -v
```

### Basic Test

```bash
# In another terminal
curl http://localhost:8080/

# Expected output:
# OK
```

---

## Command Line Options

### Synopsis

```
stitch [OPTIONS]
```

### Options

#### `-p, --port <port>`

Specify the TCP port to listen on.

- **Type:** Integer
- **Default:** 8080
- **Range:** 1-65535 (typically use 1024+ for non-root)
- **Example:** `./stitch -p 9000`

**Notes:**
- Ports below 1024 typically require root/sudo
- If port is already in use, server will fail to start with "Address already in use" error

---

#### `-h, --host <host>`

Specify the network interface to bind to.

- **Type:** IPv4 address or hostname
- **Default:** 0.0.0.0 (all interfaces)
- **Example:** `./stitch -h 127.0.0.1`

**Common Values:**
- `0.0.0.0` - Listen on all network interfaces (default)
- `127.0.0.1` - Listen only on localhost (local testing)
- `192.168.1.100` - Listen on specific IP address

**Notes:**
- Use `127.0.0.1` for security when only local testing needed
- Use `0.0.0.0` to accept connections from other machines
- IPv6 not currently supported

---

#### `-v, --verbose`

Enable verbose logging to stdout.

- **Type:** Flag (no argument)
- **Default:** Off
- **Example:** `./stitch -v`

**What Gets Logged:**
```
Stitch HTTP Negative Testing Utility
Starting server on 0.0.0.0:8080
Server listening on 0.0.0.0:8080
Press Ctrl+C to stop

Accepted new connection: fd=5
Closing connection: fd=5
Accepted new connection: fd=6
Closing connection: fd=6
```

**When to Use:**
- Debugging connection issues
- Monitoring server activity
- Understanding request flow
- Development and testing

---

#### `--help`

Display help message and exit.

- **Type:** Flag (no argument)
- **Example:** `./stitch --help`

**Output:**
```
Usage: stitch [options]
Options:
  -p, --port <port>     Port to listen on (default: 8080)
  -h, --host <host>     Host to bind to (default: 0.0.0.0)
  -v, --verbose         Enable verbose logging
  --help                Show this help message
```

---

### Combining Options

Options can be combined in any order:

```bash
# Verbose server on custom host and port
./stitch -v -h 127.0.0.1 -p 9000

# Same thing, different order
./stitch -p 9000 -v -h 127.0.0.1
```

---

## Query Parameter API

Stitch's behavior is controlled via HTTP query parameters. The general format is:

```
http://host:port/path?parameter=value&parameter2=value2
```

### Core Parameters

#### `behavior`

**Required for all non-normal responses.**

Specifies the type of test behavior to execute.

**Type:** String
**Values:** See [Behavior Types](#behavior-types)
**Example:** `?behavior=error`

---

### Behavior-Specific Parameters

Different behaviors require different additional parameters:

#### Error Responses

**Query String:** `?behavior=error&code=<code>&reason=<reason>`

- `code` (optional): HTTP status code (default: 500)
  - Type: Integer
  - Range: 100-599
  - Example: `502`

- `reason` (optional): HTTP reason phrase (default: "Internal Server Error")
  - Type: String (URL-encoded)
  - Example: `Bad%20Gateway`

---

#### Delay Behaviors

**Query String:** `?behavior=slow&delay=<milliseconds>`

- `delay` (optional): Milliseconds to delay (default: 0)
  - Type: Integer
  - Range: 0-2147483647
  - Example: `5000` (5 seconds)

---

#### Rate-Limited Sending

**Query String:** `?behavior=slow_body&rate=<bytes_per_second>`

- `rate` (optional): Bytes per second to send (default: 0 = unlimited)
  - Type: Integer
  - Range: 0-INT_MAX
  - Example: `100` (100 bytes/sec)

---

#### Partial Close

**Query String:** `?behavior=close_partial&bytes=<count>`

- `bytes` (optional): Number of bytes to send before closing (default: 0)
  - Type: Integer
  - Range: 0-INT_MAX
  - Example: `50`

---

#### Wrong Content-Length

**Query String:** `?behavior=wrong_length&length=<value>`

- `length` (optional): Incorrect Content-Length value (default: 9999)
  - Type: Integer
  - Example: `9999`

---

## Behavior Types

Complete list of supported test behaviors.

### 1. Normal Response (Default)

**No query parameters needed.**

Returns a standard HTTP 200 OK response.

```bash
curl http://localhost:8080/
```

**Response:**
```http
HTTP/1.1 200 OK
Content-Length: 2

OK
```

---

### 2. Error Response

**Parameter:** `?behavior=error`

Returns custom HTTP error response.

```bash
# Default 500 error
curl http://localhost:8080/?behavior=error

# Custom error code
curl http://localhost:8080/?behavior=error&code=502

# Custom error with reason
curl "http://localhost:8080/?behavior=error&code=502&reason=Bad%20Gateway"
```

**Response:**
```http
HTTP/1.1 502 Bad Gateway
Content-Length: 11

Bad Gateway
```

**Use Cases:**
- Test error handling in HTTP clients
- Verify retry logic
- Test error message parsing

---

### 3. Close Immediately

**Parameter:** `?behavior=close`

Closes connection immediately without sending any response.

```bash
curl http://localhost:8080/?behavior=close
```

**Behavior:**
- Connection accepted
- Connection immediately closed
- No HTTP response sent
- Client receives "Empty reply from server"

**Use Cases:**
- Test connection timeout handling
- Test client resilience to abrupt disconnects
- Simulate network failures

---

### 4. Close After Headers

**Parameter:** `?behavior=close_headers`

Sends HTTP status line and headers, then closes connection before sending body.

```bash
curl http://localhost:8080/?behavior=close_headers
```

**Behavior:**
- Sends: `HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n`
- Closes connection
- Body never sent

**Use Cases:**
- Test incomplete response handling
- Test client timeout on body wait
- Simulate server crashes mid-response

---

### 5. Close After Partial Body

**Parameter:** `?behavior=close_partial&bytes=<N>`

Sends N bytes of the response, then closes connection.

```bash
curl "http://localhost:8080/?behavior=close_partial&bytes=50"
```

**Behavior:**
- Sends first 50 bytes of complete HTTP response
- Closes connection
- May cut off mid-header or mid-body

**Use Cases:**
- Test partial response handling
- Test chunked encoding edge cases
- Simulate network interruptions

---

### 6. Slow Response

**Parameter:** `?behavior=slow&delay=<milliseconds>`

Delays before sending the entire response.

```bash
# 5 second delay
curl "http://localhost:8080/?behavior=slow&delay=5000"
```

**Behavior:**
- Connection accepted
- Waits `delay` milliseconds
- Sends complete response
- Closes connection

**Use Cases:**
- Test client timeout configuration
- Test connection pooling with slow backends
- Simulate overloaded servers

---

### 7. Slow Headers

**Parameter:** `?behavior=slow_headers&rate=<bytes_per_sec>`

Sends response headers slowly (rate-limited).

```bash
# Send at 100 bytes/second
curl "http://localhost:8080/?behavior=slow_headers&rate=100"
```

**Behavior:**
- Sends headers byte-by-byte or in small chunks
- Rate limited to `rate` bytes/second
- Body sent normally after headers complete

**Use Cases:**
- Test header timeout configuration
- Test slowloris attack mitigation
- Simulate slow network conditions

---

### 8. Slow Body

**Parameter:** `?behavior=slow_body&rate=<bytes_per_sec>`

Sends response body slowly (rate-limited).

```bash
# Send body at 50 bytes/second
curl "http://localhost:8080/?behavior=slow_body&rate=50"
```

**Behavior:**
- Sends headers normally
- Body sent in rate-limited chunks
- Rate limited to `rate` bytes/second

**Use Cases:**
- Test body read timeout configuration
- Test streaming response handling
- Simulate slow downloads

---

### 9. Invalid Status Line

**Parameter:** `?behavior=invalid_status`

Sends malformed HTTP status line.

```bash
curl http://localhost:8080/?behavior=invalid_status
```

**Response:**
```http
INVALID STATUS LINE
Content-Length: 23

Invalid status line test
```

**Behavior:**
- First line is `INVALID STATUS LINE` (not valid HTTP)
- Rest of response is properly formatted
- Most HTTP clients reject as HTTP/0.9 or error

**Use Cases:**
- Test HTTP parser robustness
- Test error recovery
- Fuzz HTTP parsers

---

### 10. Invalid Headers

**Parameter:** `?behavior=invalid_headers`

Sends malformed HTTP headers.

```bash
curl http://localhost:8080/?behavior=invalid_headers
```

**Response:**
```http
HTTP/1.1 200 OK
InvalidHeaderWithoutColon
Another Bad Header Format
Content-Length: 19

Invalid headers test
```

**Behavior:**
- Valid status line
- Some headers missing colons
- Content-Length still included

**Use Cases:**
- Test header parser strictness
- Test error recovery in HTTP clients
- Fuzz header parsing logic

---

### 11. Wrong Content-Length

**Parameter:** `?behavior=wrong_length&length=<value>`

Sends response with incorrect Content-Length header.

```bash
curl "http://localhost:8080/?behavior=wrong_length&length=9999"
```

**Response:**
```http
HTTP/1.1 200 OK
Content-Length: 9999

Wrong content length test
```

**Behavior:**
- Content-Length claims 9999 bytes
- Actual body is ~24 bytes
- Connection closed after body

**Use Cases:**
- Test Content-Length validation
- Test client behavior on length mismatch
- Test timeout when reading insufficient data

---

### 12. Malformed Chunking

**Parameter:** `?behavior=malformed_chunking`

Sends response with invalid chunked transfer encoding.

```bash
curl http://localhost:8080/?behavior=malformed_chunking
```

**Response:**
```http
HTTP/1.1 200 OK
Transfer-Encoding: chunked

INVALID_CHUNK_SIZE
Malformed chunking test
```

**Behavior:**
- Declares `Transfer-Encoding: chunked`
- Chunk size is text instead of hex number
- No proper chunk termination

**Use Cases:**
- Test chunked encoding parser
- Test recovery from bad chunks
- Fuzz chunked decoding logic

---

### 13. Timeout

**Parameter:** `?behavior=timeout`

Accepts connection but never sends a response.

```bash
curl http://localhost:8080/?behavior=timeout
# (Will hang until curl timeout)
```

**Behavior:**
- Connection accepted
- No data ever sent
- Connection held open indefinitely
- Relies on client timeout

**Use Cases:**
- Test client timeout configuration
- Test connection pool behavior
- Test resource exhaustion

---

## Usage Examples

### Basic Testing

```bash
# Normal request
curl -v http://localhost:8080/

# Custom path (ignored, but valid)
curl http://localhost:8080/test/path
```

### Error Code Testing

```bash
# Common HTTP errors
curl http://localhost:8080/?behavior=error&code=400  # Bad Request
curl http://localhost:8080/?behavior=error&code=401  # Unauthorized
curl http://localhost:8080/?behavior=error&code=403  # Forbidden
curl http://localhost:8080/?behavior=error&code=404  # Not Found
curl http://localhost:8080/?behavior=error&code=500  # Internal Server Error
curl http://localhost:8080/?behavior=error&code=502  # Bad Gateway
curl http://localhost:8080/?behavior=error&code=503  # Service Unavailable
curl http://localhost:8080/?behavior=error&code=504  # Gateway Timeout

# Custom reason phrases
curl "http://localhost:8080/?behavior=error&code=418&reason=I%27m%20a%20teapot"
curl "http://localhost:8080/?behavior=error&code=500&reason=Database%20Connection%20Failed"
```

### Connection Testing

```bash
# Test connection close handling
curl http://localhost:8080/?behavior=close

# Test partial response
curl http://localhost:8080/?behavior=close_headers
curl http://localhost:8080/?behavior=close_partial&bytes=10
curl http://localhost:8080/?behavior=close_partial&bytes=100
```

### Timeout Testing

```bash
# Test various delay scenarios
curl http://localhost:8080/?behavior=slow&delay=1000   # 1 second
curl http://localhost:8080/?behavior=slow&delay=5000   # 5 seconds
curl http://localhost:8080/?behavior=slow&delay=30000  # 30 seconds

# Test with curl timeout
curl --max-time 3 http://localhost:8080/?behavior=slow&delay=10000
# Should timeout after 3 seconds
```

### Rate Limiting Testing

```bash
# Test slow sending
curl http://localhost:8080/?behavior=slow_body&rate=10    # 10 bytes/sec
curl http://localhost:8080/?behavior=slow_body&rate=100   # 100 bytes/sec
curl http://localhost:8080/?behavior=slow_headers&rate=50 # 50 bytes/sec
```

### Malformed Response Testing

```bash
# Test various malformations
curl http://localhost:8080/?behavior=invalid_status
curl http://localhost:8080/?behavior=invalid_headers
curl http://localhost:8080/?behavior=wrong_length
curl http://localhost:8080/?behavior=malformed_chunking

# See raw responses
curl -v http://localhost:8080/?behavior=invalid_status 2>&1 | grep -A 20 "< "
```

---

## Testing Scenarios

### Scenario 1: HTTP Proxy Testing

Test how a proxy handles backend errors:

```bash
# Configure proxy to forward to Stitch
# Then test various backend failures:

# Backend returns 502
curl http://proxy/?behavior=error&code=502

# Backend times out
curl --max-time 10 http://proxy/?behavior=timeout

# Backend sends partial response
curl http://proxy/?behavior=close_partial&bytes=100
```

### Scenario 2: Load Balancer Testing

Test load balancer behavior with failing backends:

```bash
# Slow backend
curl http://lb/?behavior=slow&delay=30000

# Disconnecting backend
curl http://lb/?behavior=close

# Backend with wrong content length
curl http://lb/?behavior=wrong_length
```

### Scenario 3: Client Library Testing

Test HTTP client robustness:

```python
import requests

# Test timeout handling
try:
    requests.get('http://localhost:8080/?behavior=timeout', timeout=5)
except requests.Timeout:
    print("Correctly timed out")

# Test error handling
resp = requests.get('http://localhost:8080/?behavior=error&code=502')
assert resp.status_code == 502

# Test connection close
try:
    requests.get('http://localhost:8080/?behavior=close')
except requests.ConnectionError:
    print("Correctly handled connection close")
```

### Scenario 4: Fuzzing

Automated fuzzing of HTTP parsers:

```bash
#!/bin/bash
for behavior in error close close_headers invalid_status invalid_headers \
                wrong_length malformed_chunking; do
    echo "Testing: $behavior"
    curl -s http://localhost:8080/?behavior=$behavior > /dev/null
    echo "Status: $?"
done
```

---

## Troubleshooting

### Server Won't Start

**Error:** `Failed to bind to 0.0.0.0:8080`

**Causes:**
- Port already in use
- Insufficient permissions (ports < 1024)

**Solutions:**
```bash
# Check if port is in use
sudo netstat -tlnp | grep :8080

# Use different port
./stitch -p 9000

# Kill process using port
sudo kill <PID>
```

---

### Connection Refused

**Error:** `curl: (7) Failed to connect to localhost port 8080: Connection refused`

**Causes:**
- Server not running
- Wrong host/port
- Firewall blocking

**Solutions:**
```bash
# Check server is running
ps aux | grep stitch

# Check server is listening
netstat -tln | grep 8080

# Test with verbose
./stitch -v
```

---

### No Response / Hangs

**Behavior:** `curl` hangs indefinitely

**Causes:**
- Using `?behavior=timeout`
- Server crashed
- Network issue

**Solutions:**
```bash
# Use curl timeout
curl --max-time 5 http://localhost:8080/

# Check server logs (if -v)

# Check server is responsive
curl http://localhost:8080/
```

---

### Unexpected Response

**Problem:** Response doesn't match expected behavior

**Debug Steps:**
```bash
# 1. Use verbose mode
curl -v http://localhost:8080/?behavior=error&code=502

# 2. Check query parameters are correct
# URL encode spaces and special characters

# 3. Use raw output
curl --raw http://localhost:8080/?behavior=invalid_status | hexdump -C

# 4. Check server logs
./stitch -v
```

---

## Advanced Usage

### Using with netcat

Test raw TCP behavior:

```bash
# Send raw HTTP request
echo -e "GET /?behavior=error&code=502 HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080
```

### Using with tcpdump

Capture traffic for analysis:

```bash
# Capture traffic
sudo tcpdump -i lo -w stitch.pcap port 8080

# In another terminal
curl http://localhost:8080/?behavior=invalid_headers

# Analyze capture
wireshark stitch.pcap
```

### Automated Testing Script

```bash
#!/bin/bash
SERVER="http://localhost:8080"

echo "Testing normal response..."
curl -s $SERVER/ | grep -q "OK" && echo "✓ PASS" || echo "✗ FAIL"

echo "Testing error response..."
curl -s "$SERVER/?behavior=error&code=502" -w "%{http_code}" | grep -q "502" && echo "✓ PASS" || echo "✗ FAIL"

echo "Testing close..."
curl -s "$SERVER/?behavior=close" 2>&1 | grep -q "Empty reply" && echo "✓ PASS" || echo "✗ FAIL"

echo "All tests complete!"
```

---

## See Also

- [README.md](README.md) - Project overview and quick start
- [DESIGN.md](DESIGN.md) - Architecture and implementation details
- [Build Guide](README.md#building) - Compilation instructions
