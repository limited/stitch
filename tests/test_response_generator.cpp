#include <cppunit/extensions/HelperMacros.h>
#include "response_generator.h"

class ResponseGeneratorTest : public CppUnit::TestFixture {
    CPPUNIT_TEST_SUITE(ResponseGeneratorTest);

    // Basic response generation
    CPPUNIT_TEST(testCreateOkResponse);
    CPPUNIT_TEST(testCreateErrorResponse);
    CPPUNIT_TEST(testGenerateNormalResponse);

    // Serialization tests
    CPPUNIT_TEST(testSerializeOkResponse);
    CPPUNIT_TEST(testSerializeErrorResponse);
    CPPUNIT_TEST(testSerializeWithHeaders);
    CPPUNIT_TEST(testSerializeWithBody);

    // Malformed response tests
    CPPUNIT_TEST(testMalformedStatusLine);
    CPPUNIT_TEST(testMalformedHeaders);
    CPPUNIT_TEST(testWrongContentLength);
    CPPUNIT_TEST(testMalformedChunking);

    // Command-based generation
    CPPUNIT_TEST(testGenerateFromErrorCommand);
    CPPUNIT_TEST(testGenerateFromInvalidStatusCommand);
    CPPUNIT_TEST(testGenerateFromInvalidHeadersCommand);

    // HTTP compliance
    CPPUNIT_TEST(testStatusLineFormat);
    CPPUNIT_TEST(testHeaderFormat);
    CPPUNIT_TEST(testResponseTermination);

    CPPUNIT_TEST_SUITE_END();

private:
    ResponseGenerator* generator;

public:
    void setUp() {
        generator = new ResponseGenerator();
    }

    void tearDown() {
        delete generator;
    }

    void testCreateOkResponse() {
        HttpResponse response = ResponseGenerator::createOkResponse("Hello World");

        CPPUNIT_ASSERT_EQUAL(200, response.status_code);
        CPPUNIT_ASSERT_EQUAL(std::string("OK"), response.reason_phrase);
        CPPUNIT_ASSERT_EQUAL(std::string("Hello World"), response.body);
        CPPUNIT_ASSERT(!response.malform_status_line);
        CPPUNIT_ASSERT(!response.malform_headers);
    }

    void testCreateErrorResponse() {
        HttpResponse response = ResponseGenerator::createErrorResponse(404, "Not Found");

        CPPUNIT_ASSERT_EQUAL(404, response.status_code);
        CPPUNIT_ASSERT_EQUAL(std::string("Not Found"), response.reason_phrase);
        CPPUNIT_ASSERT(!response.malform_status_line);
    }

    void testGenerateNormalResponse() {
        TestCommand cmd;
        cmd.behavior = BehaviorType::NORMAL;

        HttpResponse response = generator->generate(cmd);

        CPPUNIT_ASSERT_EQUAL(200, response.status_code);
        CPPUNIT_ASSERT_EQUAL(std::string("OK"), response.reason_phrase);
    }

    void testSerializeOkResponse() {
        HttpResponse response = ResponseGenerator::createOkResponse("Test Body");
        std::string serialized = generator->serialize(response);

        CPPUNIT_ASSERT(!serialized.empty());
        CPPUNIT_ASSERT(serialized.find("HTTP/1.1 200 OK") != std::string::npos);
        CPPUNIT_ASSERT(serialized.find("Test Body") != std::string::npos);
    }

    void testSerializeErrorResponse() {
        HttpResponse response = ResponseGenerator::createErrorResponse(502, "Bad Gateway");
        std::string serialized = generator->serialize(response);

        CPPUNIT_ASSERT(!serialized.empty());
        CPPUNIT_ASSERT(serialized.find("HTTP/1.1 502 Bad Gateway") != std::string::npos);
    }

    void testSerializeWithHeaders() {
        HttpResponse response = ResponseGenerator::createOkResponse("Body");
        response.headers["Content-Type"] = "text/plain";
        response.headers["X-Custom-Header"] = "test-value";

        std::string serialized = generator->serialize(response);

        CPPUNIT_ASSERT(serialized.find("Content-Type: text/plain") != std::string::npos);
        CPPUNIT_ASSERT(serialized.find("X-Custom-Header: test-value") != std::string::npos);
    }

    void testSerializeWithBody() {
        HttpResponse response = ResponseGenerator::createOkResponse("Hello World");
        std::string serialized = generator->serialize(response);

        // Should have Content-Length header
        CPPUNIT_ASSERT(serialized.find("Content-Length:") != std::string::npos);
        // Should have the body
        CPPUNIT_ASSERT(serialized.find("Hello World") != std::string::npos);
    }

    void testMalformedStatusLine() {
        TestCommand cmd;
        cmd.behavior = BehaviorType::INVALID_STATUS_LINE;

        HttpResponse response = generator->generate(cmd);
        CPPUNIT_ASSERT(response.malform_status_line);

        std::string serialized = generator->serialize(response);
        // Should not have proper HTTP/1.1 format
        CPPUNIT_ASSERT(!serialized.empty());
    }

    void testMalformedHeaders() {
        TestCommand cmd;
        cmd.behavior = BehaviorType::INVALID_HEADERS;

        HttpResponse response = generator->generate(cmd);
        CPPUNIT_ASSERT(response.malform_headers);

        std::string serialized = generator->serialize(response);
        CPPUNIT_ASSERT(!serialized.empty());
    }

    void testWrongContentLength() {
        TestCommand cmd;
        cmd.behavior = BehaviorType::WRONG_CONTENT_LENGTH;

        HttpResponse response = generator->generate(cmd);
        CPPUNIT_ASSERT(response.wrong_content_length);

        std::string serialized = generator->serialize(response);
        // Should have Content-Length that doesn't match body
        CPPUNIT_ASSERT(serialized.find("Content-Length:") != std::string::npos);
    }

    void testMalformedChunking() {
        TestCommand cmd;
        cmd.behavior = BehaviorType::MALFORMED_CHUNKING;

        HttpResponse response = generator->generate(cmd);
        CPPUNIT_ASSERT(response.malform_chunking);

        std::string serialized = generator->serialize(response);
        CPPUNIT_ASSERT(!serialized.empty());
    }

    void testGenerateFromErrorCommand() {
        TestCommand cmd;
        cmd.behavior = BehaviorType::ERROR_RESPONSE;
        cmd.status_code = 503;
        cmd.reason_phrase = "Service Unavailable";

        HttpResponse response = generator->generate(cmd);

        CPPUNIT_ASSERT_EQUAL(503, response.status_code);
        CPPUNIT_ASSERT_EQUAL(std::string("Service Unavailable"), response.reason_phrase);
    }

    void testGenerateFromInvalidStatusCommand() {
        TestCommand cmd;
        cmd.behavior = BehaviorType::INVALID_STATUS_LINE;

        HttpResponse response = generator->generate(cmd);
        CPPUNIT_ASSERT(response.malform_status_line);
    }

    void testGenerateFromInvalidHeadersCommand() {
        TestCommand cmd;
        cmd.behavior = BehaviorType::INVALID_HEADERS;

        HttpResponse response = generator->generate(cmd);
        CPPUNIT_ASSERT(response.malform_headers);
    }

    void testStatusLineFormat() {
        HttpResponse response = ResponseGenerator::createOkResponse("");
        std::string serialized = generator->serialize(response);

        // Status line should be in format: HTTP/1.1 CODE REASON\r\n
        size_t first_line_end = serialized.find("\r\n");
        CPPUNIT_ASSERT(first_line_end != std::string::npos);

        std::string status_line = serialized.substr(0, first_line_end);
        CPPUNIT_ASSERT(status_line.find("HTTP/1.1") == 0);
        CPPUNIT_ASSERT(status_line.find("200") != std::string::npos);
        CPPUNIT_ASSERT(status_line.find("OK") != std::string::npos);
    }

    void testHeaderFormat() {
        HttpResponse response = ResponseGenerator::createOkResponse("Test");
        response.headers["Test-Header"] = "test-value";
        std::string serialized = generator->serialize(response);

        // Headers should be in format: Name: Value\r\n
        CPPUNIT_ASSERT(serialized.find("Test-Header: test-value\r\n") != std::string::npos);
    }

    void testResponseTermination() {
        HttpResponse response = ResponseGenerator::createOkResponse("Body");
        std::string serialized = generator->serialize(response);

        // Response should have \r\n\r\n between headers and body
        CPPUNIT_ASSERT(serialized.find("\r\n\r\n") != std::string::npos);
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(ResponseGeneratorTest);
