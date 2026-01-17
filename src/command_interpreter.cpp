#include "command_interpreter.h"
#include <algorithm>

TestCommand::TestCommand()
    : behavior(BehaviorType::NORMAL)
    , status_code(200)
    , reason_phrase("OK")
    , delay_ms(0)
    , bytes_per_second(0)
    , bytes_before_close(0)
    , body_content("OK") {
}

CommandInterpreter::CommandInterpreter() {
}

TestCommand CommandInterpreter::interpret(const std::map<std::string, std::string>& query_params) {
    TestCommand cmd;

    // Check if behavior parameter exists
    auto behavior_it = query_params.find("behavior");
    if (behavior_it == query_params.end()) {
        // No behavior specified, return default
        return cmd;
    }

    // Parse behavior type
    cmd.behavior = parseBehavior(behavior_it->second);

    // Parse additional parameters based on behavior type
    switch (cmd.behavior) {
        case BehaviorType::ERROR_RESPONSE:
            cmd.status_code = parseInteger(
                query_params.count("code") ? query_params.at("code") : "", 500);
            if (query_params.count("reason")) {
                cmd.reason_phrase = query_params.at("reason");
            } else {
                cmd.reason_phrase = "Internal Server Error";
            }
            break;

        case BehaviorType::CLOSE_AFTER_PARTIAL:
            cmd.bytes_before_close = static_cast<size_t>(parseInteger(
                query_params.count("bytes") ? query_params.at("bytes") : "", 0));
            break;

        case BehaviorType::SLOW_RESPONSE:
            cmd.delay_ms = parseInteger(
                query_params.count("delay") ? query_params.at("delay") : "", 0);
            break;

        case BehaviorType::SLOW_HEADERS:
        case BehaviorType::SLOW_BODY:
            cmd.bytes_per_second = parseInteger(
                query_params.count("rate") ? query_params.at("rate") : "", 0);
            break;

        case BehaviorType::WRONG_CONTENT_LENGTH:
            // Parse the wrong length value if provided
            if (query_params.count("length")) {
                parseInteger(query_params.at("length"), 9999);
            }
            break;

        default:
            // No additional parameters needed for other behaviors
            break;
    }

    return cmd;
}

bool CommandInterpreter::isValid(const TestCommand& cmd) const {
    // Basic validation
    if (cmd.status_code < 100 || cmd.status_code >= 600) {
        return false;
    }

    if (cmd.behavior == BehaviorType::SLOW_RESPONSE && cmd.delay_ms < 0) {
        return false;
    }

    if ((cmd.behavior == BehaviorType::SLOW_HEADERS ||
         cmd.behavior == BehaviorType::SLOW_BODY) &&
        cmd.bytes_per_second < 0) {
        return false;
    }

    return true;
}

std::string CommandInterpreter::describe(const TestCommand& cmd) const {
    switch (cmd.behavior) {
        case BehaviorType::NORMAL:
            return "Normal HTTP 200 OK response";

        case BehaviorType::ERROR_RESPONSE:
            return "Error response: " + std::to_string(cmd.status_code) +
                   " " + cmd.reason_phrase;

        case BehaviorType::CLOSE_IMMEDIATELY:
            return "Close connection immediately without response";

        case BehaviorType::CLOSE_AFTER_HEADERS:
            return "Close connection after sending headers";

        case BehaviorType::CLOSE_AFTER_PARTIAL:
            return "Close connection after sending " +
                   std::to_string(cmd.bytes_before_close) + " bytes";

        case BehaviorType::SLOW_RESPONSE:
            return "Delay response by " + std::to_string(cmd.delay_ms) + " ms";

        case BehaviorType::SLOW_HEADERS:
            return "Send headers slowly at " +
                   std::to_string(cmd.bytes_per_second) + " bytes/sec";

        case BehaviorType::SLOW_BODY:
            return "Send body slowly at " +
                   std::to_string(cmd.bytes_per_second) + " bytes/sec";

        case BehaviorType::INVALID_STATUS_LINE:
            return "Send malformed HTTP status line";

        case BehaviorType::INVALID_HEADERS:
            return "Send malformed HTTP headers";

        case BehaviorType::WRONG_CONTENT_LENGTH:
            return "Send response with incorrect Content-Length";

        case BehaviorType::MALFORMED_CHUNKING:
            return "Send response with malformed chunked encoding";

        case BehaviorType::TIMEOUT:
            return "Accept connection but never send response";

        default:
            return "Unknown behavior";
    }
}

BehaviorType CommandInterpreter::parseBehavior(const std::string& behavior_str) {
    if (behavior_str == "error") {
        return BehaviorType::ERROR_RESPONSE;
    } else if (behavior_str == "close") {
        return BehaviorType::CLOSE_IMMEDIATELY;
    } else if (behavior_str == "close_headers") {
        return BehaviorType::CLOSE_AFTER_HEADERS;
    } else if (behavior_str == "close_partial") {
        return BehaviorType::CLOSE_AFTER_PARTIAL;
    } else if (behavior_str == "slow") {
        return BehaviorType::SLOW_RESPONSE;
    } else if (behavior_str == "slow_headers") {
        return BehaviorType::SLOW_HEADERS;
    } else if (behavior_str == "slow_body") {
        return BehaviorType::SLOW_BODY;
    } else if (behavior_str == "invalid_status") {
        return BehaviorType::INVALID_STATUS_LINE;
    } else if (behavior_str == "invalid_headers") {
        return BehaviorType::INVALID_HEADERS;
    } else if (behavior_str == "wrong_length") {
        return BehaviorType::WRONG_CONTENT_LENGTH;
    } else if (behavior_str == "malformed_chunking") {
        return BehaviorType::MALFORMED_CHUNKING;
    } else if (behavior_str == "timeout") {
        return BehaviorType::TIMEOUT;
    } else {
        // Unknown behavior, default to NORMAL
        return BehaviorType::NORMAL;
    }
}

int CommandInterpreter::parseInteger(const std::string& value, int default_value) {
    if (value.empty()) {
        return default_value;
    }

    try {
        return std::stoi(value);
    } catch (...) {
        // Failed to parse, return default
        return default_value;
    }
}
