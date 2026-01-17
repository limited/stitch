#include <cppunit/extensions/HelperMacros.h>
#include "command_interpreter.h"

class CommandInterpreterTest : public CppUnit::TestFixture {
    CPPUNIT_TEST_SUITE(CommandInterpreterTest);

    // Normal behavior tests
    CPPUNIT_TEST(testNormalBehavior);
    CPPUNIT_TEST(testEmptyQueryParams);

    // Error response tests
    CPPUNIT_TEST(testErrorResponse);
    CPPUNIT_TEST(testErrorResponseWithCustomReason);
    CPPUNIT_TEST(testErrorResponseDefaults);

    // Close behavior tests
    CPPUNIT_TEST(testCloseImmediately);
    CPPUNIT_TEST(testCloseAfterHeaders);
    CPPUNIT_TEST(testCloseAfterPartial);

    // Slow response tests
    CPPUNIT_TEST(testSlowResponse);
    CPPUNIT_TEST(testSlowHeaders);
    CPPUNIT_TEST(testSlowBody);

    // Malformed response tests
    CPPUNIT_TEST(testInvalidStatusLine);
    CPPUNIT_TEST(testInvalidHeaders);
    CPPUNIT_TEST(testWrongContentLength);
    CPPUNIT_TEST(testMalformedChunking);

    // Timeout test
    CPPUNIT_TEST(testTimeout);

    // Validation tests
    CPPUNIT_TEST(testValidation);
    CPPUNIT_TEST(testDescribe);

    // Edge cases
    CPPUNIT_TEST(testUnknownBehavior);
    CPPUNIT_TEST(testInvalidParameters);

    CPPUNIT_TEST_SUITE_END();

private:
    CommandInterpreter* interpreter;

public:
    void setUp() {
        interpreter = new CommandInterpreter();
    }

    void tearDown() {
        delete interpreter;
    }

    void testNormalBehavior() {
        std::map<std::string, std::string> params;
        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::NORMAL, cmd.behavior);
        CPPUNIT_ASSERT_EQUAL(200, cmd.status_code);
        CPPUNIT_ASSERT_EQUAL(std::string("OK"), cmd.reason_phrase);
    }

    void testEmptyQueryParams() {
        std::map<std::string, std::string> params;
        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::NORMAL, cmd.behavior);
        CPPUNIT_ASSERT(interpreter->isValid(cmd));
    }

    void testErrorResponse() {
        std::map<std::string, std::string> params;
        params["behavior"] = "error";
        params["code"] = "502";

        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::ERROR_RESPONSE, cmd.behavior);
        CPPUNIT_ASSERT_EQUAL(502, cmd.status_code);
    }

    void testErrorResponseWithCustomReason() {
        std::map<std::string, std::string> params;
        params["behavior"] = "error";
        params["code"] = "502";
        params["reason"] = "Bad Gateway";

        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::ERROR_RESPONSE, cmd.behavior);
        CPPUNIT_ASSERT_EQUAL(502, cmd.status_code);
        CPPUNIT_ASSERT_EQUAL(std::string("Bad Gateway"), cmd.reason_phrase);
    }

    void testErrorResponseDefaults() {
        std::map<std::string, std::string> params;
        params["behavior"] = "error";
        // No code specified, should default to 500

        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::ERROR_RESPONSE, cmd.behavior);
        CPPUNIT_ASSERT_EQUAL(500, cmd.status_code);
    }

    void testCloseImmediately() {
        std::map<std::string, std::string> params;
        params["behavior"] = "close";

        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::CLOSE_IMMEDIATELY, cmd.behavior);
    }

    void testCloseAfterHeaders() {
        std::map<std::string, std::string> params;
        params["behavior"] = "close_headers";

        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::CLOSE_AFTER_HEADERS, cmd.behavior);
    }

    void testCloseAfterPartial() {
        std::map<std::string, std::string> params;
        params["behavior"] = "close_partial";
        params["bytes"] = "100";

        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::CLOSE_AFTER_PARTIAL, cmd.behavior);
        CPPUNIT_ASSERT_EQUAL(size_t(100), cmd.bytes_before_close);
    }

    void testSlowResponse() {
        std::map<std::string, std::string> params;
        params["behavior"] = "slow";
        params["delay"] = "5000";

        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::SLOW_RESPONSE, cmd.behavior);
        CPPUNIT_ASSERT_EQUAL(5000, cmd.delay_ms);
    }

    void testSlowHeaders() {
        std::map<std::string, std::string> params;
        params["behavior"] = "slow_headers";
        params["rate"] = "100";

        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::SLOW_HEADERS, cmd.behavior);
        CPPUNIT_ASSERT_EQUAL(100, cmd.bytes_per_second);
    }

    void testSlowBody() {
        std::map<std::string, std::string> params;
        params["behavior"] = "slow_body";
        params["rate"] = "100";

        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::SLOW_BODY, cmd.behavior);
        CPPUNIT_ASSERT_EQUAL(100, cmd.bytes_per_second);
    }

    void testInvalidStatusLine() {
        std::map<std::string, std::string> params;
        params["behavior"] = "invalid_status";

        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::INVALID_STATUS_LINE, cmd.behavior);
    }

    void testInvalidHeaders() {
        std::map<std::string, std::string> params;
        params["behavior"] = "invalid_headers";

        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::INVALID_HEADERS, cmd.behavior);
    }

    void testWrongContentLength() {
        std::map<std::string, std::string> params;
        params["behavior"] = "wrong_length";
        params["length"] = "9999";

        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::WRONG_CONTENT_LENGTH, cmd.behavior);
    }

    void testMalformedChunking() {
        std::map<std::string, std::string> params;
        params["behavior"] = "malformed_chunking";

        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::MALFORMED_CHUNKING, cmd.behavior);
    }

    void testTimeout() {
        std::map<std::string, std::string> params;
        params["behavior"] = "timeout";

        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::TIMEOUT, cmd.behavior);
    }

    void testValidation() {
        TestCommand cmd;
        cmd.behavior = BehaviorType::NORMAL;
        cmd.status_code = 200;

        CPPUNIT_ASSERT(interpreter->isValid(cmd));
    }

    void testDescribe() {
        TestCommand cmd;
        cmd.behavior = BehaviorType::ERROR_RESPONSE;
        cmd.status_code = 502;
        cmd.reason_phrase = "Bad Gateway";

        std::string description = interpreter->describe(cmd);
        CPPUNIT_ASSERT(!description.empty());
    }

    void testUnknownBehavior() {
        std::map<std::string, std::string> params;
        params["behavior"] = "unknown_behavior_xyz";

        TestCommand cmd = interpreter->interpret(params);

        // Should default to NORMAL for unknown behavior
        CPPUNIT_ASSERT_EQUAL(BehaviorType::NORMAL, cmd.behavior);
    }

    void testInvalidParameters() {
        std::map<std::string, std::string> params;
        params["behavior"] = "slow";
        params["delay"] = "not_a_number";

        TestCommand cmd = interpreter->interpret(params);

        CPPUNIT_ASSERT_EQUAL(BehaviorType::SLOW_RESPONSE, cmd.behavior);
        // Should use default value (0) for invalid number
        CPPUNIT_ASSERT_EQUAL(0, cmd.delay_ms);
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(CommandInterpreterTest);
