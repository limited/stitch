#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <string>
#include <map>

class HttpRequest {
public:
    std::string method;
    std::string path;
    std::string http_version;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query_params;

    bool isValid() const;
};

class HttpParser {
public:
    enum class ParseResult {
        COMPLETE,       // Request fully parsed
        INCOMPLETE,     // Need more data
        ERROR           // Parse error
    };

    HttpParser();

    // Feed data into parser, returns parse status
    ParseResult parse(const char* data, size_t length);

    // Get parsed request (only valid if parse returned COMPLETE)
    const HttpRequest& getRequest() const;

    // Reset parser for next request
    void reset();

    // Get error message if parse failed
    std::string getErrorMessage() const;

private:
    HttpRequest request_;
    std::string buffer_;
    ParseResult state_;
    std::string error_message_;
    bool headers_complete_;

    bool parseRequestLine(const std::string& line);
    bool parseHeader(const std::string& line);
    void extractQueryParams(const std::string& path);
    std::string urlDecode(const std::string& str);
    size_t findEndOfHeaders(const std::string& buffer);
};

#endif // HTTP_PARSER_H
