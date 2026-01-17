#include "http_parser.h"
#include <sstream>
#include <algorithm>
#include <cctype>

// HttpRequest implementation
bool HttpRequest::isValid() const {
    return !method.empty() && !path.empty() && !http_version.empty();
}

// HttpParser implementation
HttpParser::HttpParser()
    : state_(ParseResult::INCOMPLETE)
    , headers_complete_(false) {
}

void HttpParser::reset() {
    request_ = HttpRequest();
    buffer_.clear();
    state_ = ParseResult::INCOMPLETE;
    error_message_.clear();
    headers_complete_ = false;
}

HttpParser::ParseResult HttpParser::parse(const char* data, size_t length) {
    if (state_ == ParseResult::COMPLETE || state_ == ParseResult::ERROR) {
        return state_;
    }

    // Append new data to buffer
    buffer_.append(data, length);

    // Check if we have complete headers (terminated by \r\n\r\n)
    size_t headers_end = findEndOfHeaders(buffer_);
    if (headers_end == std::string::npos) {
        // Need more data
        state_ = ParseResult::INCOMPLETE;
        return state_;
    }

    // Extract the headers section
    std::string headers_section = buffer_.substr(0, headers_end);

    // Split into lines
    std::istringstream stream(headers_section);
    std::string line;
    bool first_line = true;

    while (std::getline(stream, line)) {
        // Remove \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            continue;
        }

        if (first_line) {
            if (!parseRequestLine(line)) {
                state_ = ParseResult::ERROR;
                return state_;
            }
            first_line = false;
        } else {
            if (!parseHeader(line)) {
                state_ = ParseResult::ERROR;
                return state_;
            }
        }
    }

    // Extract query parameters from path
    extractQueryParams(request_.path);

    state_ = ParseResult::COMPLETE;
    return state_;
}

const HttpRequest& HttpParser::getRequest() const {
    return request_;
}

const std::string& HttpParser::getErrorMessage() const {
    return error_message_;
}

bool HttpParser::parseRequestLine(const std::string& line) {
    // Request line format: METHOD PATH HTTP/VERSION
    std::istringstream iss(line);
    std::string method, path, version;

    if (!(iss >> method >> path >> version)) {
        error_message_ = "Invalid request line format";
        return false;
    }

    // Validate HTTP version format
    if (version.substr(0, 5) != "HTTP/") {
        error_message_ = "Invalid HTTP version";
        return false;
    }

    request_.method = method;
    request_.path = path;
    request_.http_version = version;

    return true;
}

bool HttpParser::parseHeader(const std::string& line) {
    // Header format: Name: Value
    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
        error_message_ = "Invalid header format (missing colon)";
        return false;
    }

    std::string name = line.substr(0, colon_pos);
    std::string value = line.substr(colon_pos + 1);

    // Trim leading whitespace from value
    size_t value_start = value.find_first_not_of(" \t");
    if (value_start != std::string::npos) {
        value = value.substr(value_start);
    } else {
        value.clear();
    }

    request_.headers[name] = value;
    return true;
}

void HttpParser::extractQueryParams(const std::string& path) {
    // Find the query string (after '?')
    size_t query_start = path.find('?');
    if (query_start == std::string::npos) {
        return;
    }

    std::string query_string = path.substr(query_start + 1);

    // Split by '&'
    size_t pos = 0;
    while (pos < query_string.length()) {
        size_t next_amp = query_string.find('&', pos);
        size_t end = (next_amp == std::string::npos) ? query_string.length() : next_amp;

        std::string param = query_string.substr(pos, end - pos);

        // Split by '='
        size_t equals = param.find('=');
        if (equals != std::string::npos) {
            std::string key = param.substr(0, equals);
            std::string value = param.substr(equals + 1);

            // URL decode the value
            request_.query_params[key] = urlDecode(value);
        } else {
            // Parameter without value
            request_.query_params[param] = "";
        }

        pos = (next_amp == std::string::npos) ? query_string.length() : next_amp + 1;
    }
}

std::string HttpParser::urlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.length());

    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%') {
            if (i + 2 < str.length()) {
                // Convert hex digits to character
                std::string hex = str.substr(i + 1, 2);
                char ch = static_cast<char>(std::stoi(hex, nullptr, 16));
                result += ch;
                i += 2;
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }

    return result;
}

size_t HttpParser::findEndOfHeaders(const std::string& buffer) {
    // Look for \r\n\r\n
    size_t pos = buffer.find("\r\n\r\n");
    if (pos != std::string::npos) {
        return pos + 4;  // Include the \r\n\r\n
    }
    return std::string::npos;
}
