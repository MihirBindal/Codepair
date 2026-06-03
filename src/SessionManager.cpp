#include "SessionManager.h"

string SessionManager::generate_id() {
    string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    random_device rd;
    mt19937 generator(rd());
    uniform_int_distribution<> distribution(0, chars.size() - 1);
    string id = "";
    for (int i = 0; i < 6; ++i) {
        id += chars[distribution(generator)];
    }
    return id;
}

string SessionManager::create_session() {
    unique_lock<shared_mutex> lock(rw_mutex);
    string id;
    do {
        id = generate_id();
    } while (sessions.find(id) != sessions.end());
    
    Session s;
    s.id = id;
    sessions[id] = s;
    return id;
}

bool SessionManager::join(string id, void* ws, string role) {
    unique_lock<shared_mutex> lock(rw_mutex);
    auto it = sessions.find(id);
    if (it == sessions.end()) return false;
    
    if (role == "interviewer") {
        it->second.interviewer = ws;
    } else if (role == "candidate") {
        it->second.candidate = ws;
    } else {
        return false;
    }
    return true;
}

void SessionManager::leave(string id, void* ws) {
    unique_lock<shared_mutex> lock(rw_mutex);
    auto it = sessions.find(id);
    if (it == sessions.end()) return;
    
    if (it->second.interviewer == ws) {
        it->second.interviewer = nullptr;
    }
    if (it->second.candidate == ws) {
        it->second.candidate = nullptr;
    }
}

bool SessionManager::get_session(string id, Session& out_session) {
    shared_lock<shared_mutex> lock(rw_mutex);
    auto it = sessions.find(id);
    if (it == sessions.end()) return false;
    out_session = it->second;
    return true;
}

void SessionManager::update_code(string id, string code) {
    unique_lock<shared_mutex> lock(rw_mutex);
    auto it = sessions.find(id);
    if (it != sessions.end()) {
        it->second.code = code;
    }
}

void SessionManager::update_problem(string id, string problem) {
    unique_lock<shared_mutex> lock(rw_mutex);
    auto it = sessions.find(id);
    if (it != sessions.end()) {
        it->second.problem = problem;
    }
}

void SessionManager::update_language(string id, string language) {
    unique_lock<shared_mutex> lock(rw_mutex);
    auto it = sessions.find(id);
    if (it != sessions.end()) {
        it->second.language = language;
    }
}
