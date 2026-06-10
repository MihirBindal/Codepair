#pragma once
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <random>
#include <mutex>
#include <chrono>
#include <vector>

using namespace std;

struct Session {
    string id;
    string code;
    string problem;
    string language = "cpp";
    string stdin_input;
    int version = 0;
    chrono::steady_clock::time_point last_activity = chrono::steady_clock::now();
    void* interviewer = nullptr;
    void* candidate = nullptr;
};

class SessionManager {
public:
    string create_session();
    bool join(string id, void* ws, string role);
    void leave(string id, void* ws);
    bool get_session(string id, Session& out_session);
    void update_code(string id, string code);
    bool update_code_with_version(string id, string code, int version);
    void update_problem(string id, string problem);
    void update_language(string id, string language);
    void update_stdin(string id, string stdin_input);
    void touch(string id);
    vector<string> get_expired_sessions(int expiry_hours);
    void delete_session(string id);
private:
    unordered_map<string, Session> sessions;
    shared_mutex rw_mutex;
    string generate_id();
};

