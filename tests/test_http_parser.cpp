#include <cppunit/extensions/HelperMacros.h>
#include <cstring>
#include "http_parser.h"

class HttpParserTest : public CppUnit::TestFixture {
    CPPUNIT_TEST_SUITE(HttpParserTest);

    // Basic parsing tests
    CPPUNIT_TEST(testSimpleGetRequest);
    CPPUNIT_TEST(testGetRequestWithHeaders);
    CPPUNIT_TEST(testPostRequest);

    // Query parameter tests
    CPPUNIT_TEST(testQueryParameterExtraction);
    CPPUNIT_TEST(testMultipleQueryParameters);
    CPPUNIT_TEST(testUrlEncodedQueryParameters);
    CPPUNIT_TEST(testNoQueryParameters);

    // Partial request tests
    CPPUNIT_TEST(testPartialRequestLine);
    CPPUNIT_TEST(testPartialHeaders);
    CPPUNIT_TEST(testIncrementalParsing);

    // Edge cases and malformed requests
    CPPUNIT_TEST(testMalformedRequestLine);
    CPPUNIT_TEST(testMalformedHeaders);
    CPPUNIT_TEST(testEmptyRequest);
    CPPUNIT_TEST(testVeryLongHeaders);

    // HTTP version tests
    CPPUNIT_TEST(testHttp10Request);
    CPPUNIT_TEST(testHttp11Request);

    // Reset functionality
    CPPUNIT_TEST(testParserReset);

    CPPUNIT_TEST_SUITE_END();

private:
    HttpParser* parser;

public:
    void setUp() {
        parser = new HttpParser();
    }

    void tearDown() {
        delete parser;
    }

    void testSimpleGetRequest() {
        const char* request = "GET / HTTP/1.1\r\n\r\n";
        HttpParser::ParseResult result = parser->parse(request, strlen(request));

        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::COMPLETE, result);
        const HttpRequest& req = parser->getRequest();
        CPPUNIT_ASSERT_EQUAL(std::string("GET"), req.method);
        CPPUNIT_ASSERT_EQUAL(std::string("/"), req.path);
        CPPUNIT_ASSERT_EQUAL(std::string("HTTP/1.1"), req.http_version);
        CPPUNIT_ASSERT(req.isValid());
    }

    void testGetRequestWithHeaders() {
        const char* request =
            "GET /test HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "User-Agent: TestClient/1.0\r\n"
            "Accept: */*\r\n"
            "\r\n";

        HttpParser::ParseResult result = parser->parse(request, strlen(request));

        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::COMPLETE, result);
        const HttpRequest& req = parser->getRequest();
        CPPUNIT_ASSERT_EQUAL(std::string("GET"), req.method);
        CPPUNIT_ASSERT_EQUAL(std::string("/test"), req.path);
        CPPUNIT_ASSERT_EQUAL(std::string("HTTP/1.1"), req.http_version);

        CPPUNIT_ASSERT_EQUAL(std::string("example.com"), req.headers.at("Host"));
        CPPUNIT_ASSERT_EQUAL(std::string("TestClient/1.0"), req.headers.at("User-Agent"));
        CPPUNIT_ASSERT_EQUAL(std::string("*/*"), req.headers.at("Accept"));
    }

    void testPostRequest() {
        const char* request =
            "POST /api/data HTTP/1.1\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: 0\r\n"
            "\r\n";

        HttpParser::ParseResult result = parser->parse(request, strlen(request));

        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::COMPLETE, result);
        const HttpRequest& req = parser->getRequest();
        CPPUNIT_ASSERT_EQUAL(std::string("POST"), req.method);
        CPPUNIT_ASSERT_EQUAL(std::string("/api/data"), req.path);
    }

    void testQueryParameterExtraction() {
        const char* request = "GET /test?behavior=error HTTP/1.1\r\n\r\n";
        HttpParser::ParseResult result = parser->parse(request, strlen(request));

        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::COMPLETE, result);
        const HttpRequest& req = parser->getRequest();
        CPPUNIT_ASSERT_EQUAL(std::string("/test?behavior=error"), req.path);
        CPPUNIT_ASSERT_EQUAL(std::string("error"), req.query_params.at("behavior"));
    }

    void testMultipleQueryParameters() {
        const char* request = "GET /?behavior=error&code=502&delay=1000 HTTP/1.1\r\n\r\n";
        HttpParser::ParseResult result = parser->parse(request, strlen(request));

        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::COMPLETE, result);
        const HttpRequest& req = parser->getRequest();
        CPPUNIT_ASSERT_EQUAL(std::string("error"), req.query_params.at("behavior"));
        CPPUNIT_ASSERT_EQUAL(std::string("502"), req.query_params.at("code"));
        CPPUNIT_ASSERT_EQUAL(std::string("1000"), req.query_params.at("delay"));
    }

    void testUrlEncodedQueryParameters() {
        const char* request = "GET /?reason=Bad%20Gateway&message=test%2Bvalue HTTP/1.1\r\n\r\n";
        HttpParser::ParseResult result = parser->parse(request, strlen(request));

        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::COMPLETE, result);
        const HttpRequest& req = parser->getRequest();
        CPPUNIT_ASSERT_EQUAL(std::string("Bad Gateway"), req.query_params.at("reason"));
        CPPUNIT_ASSERT_EQUAL(std::string("test+value"), req.query_params.at("message"));
    }

    void testNoQueryParameters() {
        const char* request = "GET / HTTP/1.1\r\n\r\n";
        HttpParser::ParseResult result = parser->parse(request, strlen(request));

        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::COMPLETE, result);
        const HttpRequest& req = parser->getRequest();
        CPPUNIT_ASSERT(req.query_params.empty());
    }

    void testPartialRequestLine() {
        const char* partial = "GET /test";
        HttpParser::ParseResult result = parser->parse(partial, strlen(partial));

        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::INCOMPLETE, result);
    }

    void testPartialHeaders() {
        const char* partial = "GET / HTTP/1.1\r\nHost: example.com\r\n";
        HttpParser::ParseResult result = parser->parse(partial, strlen(partial));

        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::INCOMPLETE, result);
    }

    void testIncrementalParsing() {
        const char* part1 = "GET /test HTTP/1.1\r\n";
        const char* part2 = "Host: example.com\r\n";
        const char* part3 = "\r\n";

        HttpParser::ParseResult result1 = parser->parse(part1, strlen(part1));
        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::INCOMPLETE, result1);

        HttpParser::ParseResult result2 = parser->parse(part2, strlen(part2));
        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::INCOMPLETE, result2);

        HttpParser::ParseResult result3 = parser->parse(part3, strlen(part3));
        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::COMPLETE, result3);

        const HttpRequest& req = parser->getRequest();
        CPPUNIT_ASSERT_EQUAL(std::string("GET"), req.method);
        CPPUNIT_ASSERT_EQUAL(std::string("/test"), req.path);
        CPPUNIT_ASSERT_EQUAL(std::string("example.com"), req.headers.at("Host"));
    }

    void testMalformedRequestLine() {
        const char* request = "INVALID REQUEST\r\n\r\n";
        HttpParser::ParseResult result = parser->parse(request, strlen(request));

        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::ERROR, result);
        CPPUNIT_ASSERT(!parser->getErrorMessage().empty());
    }

    void testMalformedHeaders() {
        const char* request = "GET / HTTP/1.1\r\nInvalidHeaderNoColon\r\n\r\n";
        HttpParser::ParseResult result = parser->parse(request, strlen(request));

        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::ERROR, result);
    }

    void testEmptyRequest() {
        const char* request = "";
        HttpParser::ParseResult result = parser->parse(request, 0);

        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::INCOMPLETE, result);
    }

    void testVeryLongHeaders() {
        std::string request = "GET / HTTP/1.1\r\n";
        request += "X-Long-Header: ";
        request += std::string(8000, 'A');  // 8KB header value
        request += "\r\n\r\n";

        HttpParser::ParseResult result = parser->parse(request.c_str(), request.length());

        // Should either complete successfully or return error (implementation dependent)
        CPPUNIT_ASSERT(result == HttpParser::ParseResult::COMPLETE ||
                       result == HttpParser::ParseResult::ERROR);
    }

    void testHttp10Request() {
        const char* request = "GET / HTTP/1.0\r\n\r\n";
        HttpParser::ParseResult result = parser->parse(request, strlen(request));

        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::COMPLETE, result);
        const HttpRequest& req = parser->getRequest();
        CPPUNIT_ASSERT_EQUAL(std::string("HTTP/1.0"), req.http_version);
    }

    void testHttp11Request() {
        const char* request = "GET / HTTP/1.1\r\n\r\n";
        HttpParser::ParseResult result = parser->parse(request, strlen(request));

        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::COMPLETE, result);
        const HttpRequest& req = parser->getRequest();
        CPPUNIT_ASSERT_EQUAL(std::string("HTTP/1.1"), req.http_version);
    }

    void testParserReset() {
        const char* request1 = "GET /first HTTP/1.1\r\n\r\n";
        parser->parse(request1, strlen(request1));

        parser->reset();

        const char* request2 = "POST /second HTTP/1.1\r\n\r\n";
        HttpParser::ParseResult result = parser->parse(request2, strlen(request2));

        CPPUNIT_ASSERT_EQUAL(HttpParser::ParseResult::COMPLETE, result);
        const HttpRequest& req = parser->getRequest();
        CPPUNIT_ASSERT_EQUAL(std::string("POST"), req.method);
        CPPUNIT_ASSERT_EQUAL(std::string("/second"), req.path);
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(HttpParserTest);
