#pragma once
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <random>
#include <mutex>

using namespace std;

struct Session {
    string id;
    string code;
    string problem;
    string language = "cpp";
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
    void update_problem(string id, string problem);
    void update_language(string id, string language);
private:
    unordered_map<string, Session> sessions;
    shared_mutex rw_mutex;
    string generate_id();
};
