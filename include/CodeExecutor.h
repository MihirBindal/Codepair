#pragma once
#include <string>

using namespace std;

struct ExecutionResult {
    string stdout_output;
    string stderr_output;
    int exit_code = 0;
    bool timed_out = false;
};

class CodeExecutor {
public:
    ExecutionResult execute(string code, string language);
};
