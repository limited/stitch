#ifndef RESPONSE_GENERATOR_H
#define RESPONSE_GENERATOR_H

#include <string>
#include <map>
#include "command_interpreter.h"

struct HttpResponse {
    int status_code;
    std::string reason_phrase;
    std::map<std::string, std::string> headers;
    std::string body;

    bool malform_status_line;
    bool malform_headers;
    bool wrong_content_length;
    int wrong_content_length_value;
    bool malform_chunking;
};

class ResponseGenerator {
public:
    ResponseGenerator();
    HttpResponse generate(const TestCommand& cmd);
    std::string serialize(const HttpResponse& response);

    static HttpResponse createOkResponse(const std::string& body);
    static HttpResponse createErrorResponse(int code, const std::string& reason);
    static HttpResponse createMalformedResponse(const TestCommand& cmd);

private:
    std::string serializeStatusLine(const HttpResponse& response);
    std::string serializeHeaders(const HttpResponse& response);
    std::string serializeBody(const HttpResponse& response);
};

#endif // RESPONSE_GENERATOR_H
