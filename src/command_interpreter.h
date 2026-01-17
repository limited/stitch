#ifndef COMMAND_INTERPRETER_H
#define COMMAND_INTERPRETER_H

#include <string>
#include <map>

enum class BehaviorType {
    NORMAL,
    ERROR_RESPONSE,
    CLOSE_IMMEDIATELY,
    CLOSE_AFTER_HEADERS,
    CLOSE_AFTER_PARTIAL,
    SLOW_RESPONSE,
    SLOW_HEADERS,
    SLOW_BODY,
    INVALID_STATUS_LINE,
    INVALID_HEADERS,
    WRONG_CONTENT_LENGTH,
    MALFORMED_CHUNKING,
    TIMEOUT
};

struct TestCommand {
    BehaviorType behavior;
    int status_code;
    std::string reason_phrase;
    int delay_ms;
    int bytes_per_second;
    size_t bytes_before_close;
    std::string body_content;

    TestCommand();
};

class CommandInterpreter {
public:
    CommandInterpreter();
    TestCommand interpret(const std::map<std::string, std::string>& query_params);
    bool isValid(const TestCommand& cmd) const;
    std::string describe(const TestCommand& cmd) const;

private:
    BehaviorType parseBehavior(const std::string& behavior_str);
    int parseInteger(const std::string& value, int default_value);
};

#endif // COMMAND_INTERPRETER_H
