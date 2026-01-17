#include "response_generator.h"
#include <sstream>

ResponseGenerator::ResponseGenerator() {
}

HttpResponse ResponseGenerator::generate(const TestCommand& cmd) {
    HttpResponse response;

    // Initialize with defaults
    response.malform_status_line = false;
    response.malform_headers = false;
    response.wrong_content_length = false;
    response.wrong_content_length_value = 0;
    response.malform_chunking = false;

    switch (cmd.behavior) {
        case BehaviorType::NORMAL:
            response.status_code = 200;
            response.reason_phrase = "OK";
            response.body = cmd.body_content;
            break;

        case BehaviorType::ERROR_RESPONSE:
            response.status_code = cmd.status_code;
            response.reason_phrase = cmd.reason_phrase;
            response.body = cmd.reason_phrase;
            break;

        case BehaviorType::INVALID_STATUS_LINE:
            response.status_code = 200;
            response.reason_phrase = "OK";
            response.body = "Invalid status line test";
            response.malform_status_line = true;
            break;

        case BehaviorType::INVALID_HEADERS:
            response.status_code = 200;
            response.reason_phrase = "OK";
            response.body = "Invalid headers test";
            response.malform_headers = true;
            break;

        case BehaviorType::WRONG_CONTENT_LENGTH:
            response.status_code = 200;
            response.reason_phrase = "OK";
            response.body = "Wrong content length test";
            response.wrong_content_length = true;
            response.wrong_content_length_value = 9999;
            break;

        case BehaviorType::MALFORMED_CHUNKING:
            response.status_code = 200;
            response.reason_phrase = "OK";
            response.body = "Malformed chunking test";
            response.malform_chunking = true;
            break;

        default:
            // For other behaviors (close, slow, timeout), generate normal response
            // The behavior will be handled by ConnectionHandler
            response.status_code = 200;
            response.reason_phrase = "OK";
            response.body = cmd.body_content;
            break;
    }

    return response;
}

std::string ResponseGenerator::serialize(const HttpResponse& response) {
    std::string result;

    // Serialize status line
    result += serializeStatusLine(response);

    // Serialize headers
    result += serializeHeaders(response);

    // End of headers
    result += "\r\n";

    // Serialize body
    result += serializeBody(response);

    return result;
}

HttpResponse ResponseGenerator::createOkResponse(const std::string& body) {
    HttpResponse response;
    response.status_code = 200;
    response.reason_phrase = "OK";
    response.body = body;
    response.malform_status_line = false;
    response.malform_headers = false;
    response.wrong_content_length = false;
    response.wrong_content_length_value = 0;
    response.malform_chunking = false;
    return response;
}

HttpResponse ResponseGenerator::createErrorResponse(int code, const std::string& reason) {
    HttpResponse response;
    response.status_code = code;
    response.reason_phrase = reason;
    response.body = reason;
    response.malform_status_line = false;
    response.malform_headers = false;
    response.wrong_content_length = false;
    response.wrong_content_length_value = 0;
    response.malform_chunking = false;
    return response;
}

HttpResponse ResponseGenerator::createMalformedResponse(const TestCommand& cmd) {
    HttpResponse response;
    response.status_code = 200;
    response.reason_phrase = "OK";
    response.body = "Malformed";
    response.malform_status_line = true;
    response.malform_headers = false;
    response.wrong_content_length = false;
    response.wrong_content_length_value = 0;
    response.malform_chunking = false;
    return response;
}

std::string ResponseGenerator::serializeStatusLine(const HttpResponse& response) {
    if (response.malform_status_line) {
        // Return malformed status line (missing HTTP version, wrong format, etc.)
        return "INVALID STATUS LINE\r\n";
    }

    std::ostringstream oss;
    oss << "HTTP/1.1 " << response.status_code << " " << response.reason_phrase << "\r\n";
    return oss.str();
}

std::string ResponseGenerator::serializeHeaders(const HttpResponse& response) {
    std::ostringstream oss;

    // Add custom headers first
    if (response.malform_headers) {
        // Add some malformed headers (missing colon, wrong format, etc.)
        oss << "InvalidHeaderWithoutColon\r\n";
        oss << "Another Bad Header Format\r\n";
    } else {
        // Add user-specified headers
        for (const auto& header : response.headers) {
            oss << header.first << ": " << header.second << "\r\n";
        }
    }

    // Add Content-Length header
    if (!response.body.empty()) {
        if (response.wrong_content_length) {
            oss << "Content-Length: " << response.wrong_content_length_value << "\r\n";
        } else if (response.malform_chunking) {
            // Use chunked encoding header
            oss << "Transfer-Encoding: chunked\r\n";
        } else {
            oss << "Content-Length: " << response.body.length() << "\r\n";
        }
    }

    return oss.str();
}

std::string ResponseGenerator::serializeBody(const HttpResponse& response) {
    if (response.malform_chunking) {
        // Return malformed chunked encoding
        // Valid format should be: <hex-size>\r\n<data>\r\n0\r\n\r\n
        // We'll send something invalid
        return "INVALID_CHUNK_SIZE\r\n" + response.body + "\r\n";
    }

    return response.body;
}
