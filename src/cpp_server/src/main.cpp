#include <App.h>
#include <iostream>
#include <string_view>
#include <string>
#include <memory>
#include <chrono>
#include <vector>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#include "SessionManager.h"
#include "RedisClient.h"
#include "ThreadPool.h"

using namespace std;

struct SocketData {
    string room_id;
    string role;
    shared_ptr<bool> run_aborted;
};

struct RunRequest {
    string room_id;
    string code;
    string language;
    string input;
};

struct ExecutionResult {
    string stdout_output;
    string stderr_output;
    int exit_code = 0;
    bool timed_out = false;
};

struct WSMessage {
    string type;
    string code;
    int version = -1;
    string language;
    string problem;
    string input;
};

SessionManager session_manager;
RedisClient redis_client;
ThreadPool thread_pool(4);

// Escape a string to be safely embedded in JSON
string escape_json(const string& s) {
    string res = "";
    for (char c : s) {
        if (c == '"') res += "\\\"";
        else if (c == '\\') res += "\\\\";
        else if (c == '\n') res += "\\n";
        else if (c == '\r') res += "\\r";
        else if (c == '\t') res += "\\t";
        else res += c;
    }
    return res;
}

// Helper to find a specific JSON key in a simple flat JSON string, ignoring matching string values
size_t find_json_key(const string& json, const string& key) {
    string search_str = "\"" + key + "\"";
    size_t pos = 0;
    while (true) {
        pos = json.find(search_str, pos);
        if (pos == string::npos) return string::npos;
        size_t after_quote = pos + search_str.length();
        // Skip whitespace
        while (after_quote < json.length() && (json[after_quote] == ' ' || json[after_quote] == '\t' || json[after_quote] == '\r' || json[after_quote] == '\n')) {
            after_quote++;
        }
        if (after_quote < json.length() && json[after_quote] == ':') {
            return pos; // Found the actual key
        }
        pos += search_str.length(); // Skip and search next
    }
}

// Simple JSON parser for WebSocket and HTTP payloads
WSMessage parse_ws_message(const string& text) {
    WSMessage msg;
    // Extract "type"
    size_t type_pos = find_json_key(text, "type");
    if (type_pos != string::npos) {
        size_t start = text.find("\"", type_pos + 6);
        if (start != string::npos) {
            size_t end = text.find("\"", start + 1);
            if (end != string::npos) msg.type = text.substr(start + 1, end - start - 1);
        }
    }
    // Extract "code"
    size_t code_pos = find_json_key(text, "code");
    if (code_pos != string::npos) {
        size_t start = text.find("\"", code_pos + 6);
        if (start != string::npos) {
            string code_val = "";
            bool escaped = false;
            size_t i = start + 1;
            while (i < text.length()) {
                char c = text[i];
                if (escaped) {
                    if (c == 'n') code_val += '\n';
                    else if (c == 't') code_val += '\t';
                    else if (c == '\"') code_val += '\"';
                    else if (c == '\\') code_val += '\\';
                    else code_val += c;
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                } else if (c == '\"') {
                    break;
                } else {
                    code_val += c;
                }
                i++;
            }
            msg.code = code_val;
        }
    }
    // Extract "version"
    size_t ver_pos = find_json_key(text, "version");
    if (ver_pos != string::npos) {
        size_t start = text.find(":", ver_pos + 9);
        if (start != string::npos) {
            size_t i = start + 1;
            while (i < text.length() && (text[i] == ' ' || text[i] == '\t' || text[i] == ':')) i++;
            string num_str = "";
            while (i < text.length() && isdigit(text[i])) {
                num_str += text[i];
                i++;
            }
            if (!num_str.empty()) msg.version = stoi(num_str);
        }
    }
    // Extract "language"
    size_t lang_pos = find_json_key(text, "language");
    if (lang_pos != string::npos) {
        size_t start = text.find("\"", lang_pos + 10);
        if (start != string::npos) {
            size_t end = text.find("\"", start + 1);
            if (end != string::npos) msg.language = text.substr(start + 1, end - start - 1);
        }
    }
    // Extract "problem"
    size_t prob_pos = find_json_key(text, "problem");
    if (prob_pos != string::npos) {
        size_t start = text.find("\"", prob_pos + 9);
        if (start != string::npos) {
            string prob_val = "";
            bool escaped = false;
            size_t i = start + 1;
            while (i < text.length()) {
                char c = text[i];
                if (escaped) {
                    if (c == 'n') prob_val += '\n';
                    else if (c == 't') prob_val += '\t';
                    else if (c == '\"') prob_val += '\"';
                    else if (c == '\\') prob_val += '\\';
                    else prob_val += c;
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                } else if (c == '\"') {
                    break;
                } else {
                    prob_val += c;
                }
                i++;
            }
            msg.problem = prob_val;
        }
    }
    // Extract "input"
    size_t input_pos = find_json_key(text, "input");
    if (input_pos != string::npos) {
        size_t start = text.find("\"", input_pos + 7);
        if (start != string::npos) {
            string input_val = "";
            bool escaped = false;
            size_t i = start + 1;
            while (i < text.length()) {
                char c = text[i];
                if (escaped) {
                    if (c == 'n') input_val += '\n';
                    else if (c == 't') input_val += '\t';
                    else if (c == '\"') input_val += '\"';
                    else if (c == '\\') input_val += '\\';
                    else input_val += c;
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                } else if (c == '\"') {
                    break;
                } else {
                    input_val += c;
                }
                i++;
            }
            msg.input = input_val;
        }
    }
    return msg;
}

RunRequest parse_run_request(const string& body) {
    WSMessage parsed = parse_ws_message(body);
    RunRequest req;
    req.code = parsed.code;
    req.language = parsed.language;
    req.input = parsed.input;
    
    // Extract "room_id"
    size_t room_pos = body.find("\"room_id\"");
    if (room_pos != string::npos) {
        size_t start = body.find("\"", room_pos + 9);
        if (start != string::npos) {
            size_t end = body.find("\"", start + 1);
            if (end != string::npos) req.room_id = body.substr(start + 1, end - start - 1);
        }
    }
    return req;
}

// HTTP/1.0 streaming client connecting to Python FastAPI sidecar
ExecutionResult run_code_via_sidecar(const string& code, const string& language, const string& input, const string& ws_room_id, uWS::Loop* loop, shared_ptr<bool> aborted) {
    ExecutionResult res;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        res.stderr_output = "Failed to create execution socket";
        res.exit_code = 1;
        return res;
    }
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(4002);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        res.stderr_output = "Invalid sidecar target address";
        res.exit_code = 1;
        close(sock);
        return res;
    }
    
    // Set socket timeout (10 seconds)
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        res.stderr_output = "Failed to connect to execution sidecar microservice";
        res.exit_code = 1;
        close(sock);
        return res;
    }
    
    string payload = "{\"code\":\"" + escape_json(code) + "\",\"language\":\"" + escape_json(language) + "\",\"input\":\"" + escape_json(input) + "\"}";
    string req = "POST /execute HTTP/1.0\r\n"
                 "Host: 127.0.0.1:4002\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: " + to_string(payload.length()) + "\r\n"
                 "Connection: close\r\n\r\n" + payload;
                 
    if (send(sock, req.c_str(), req.length(), 0) < 0) {
        res.stderr_output = "Failed to transmit request to sidecar";
        res.exit_code = 1;
        close(sock);
        return res;
    }
    
    char buf[4096];
    string response_accumulator = "";
    bool headers_passed = false;
    string stream_buffer = "";
    
    auto parse_json_field = [](const string& line, const string& key) {
        size_t pos = line.find("\"" + key + "\"");
        if (pos == string::npos) return string("");
        size_t colon = line.find(":", pos);
        if (colon == string::npos) return string("");
        
        size_t start = line.find("\"", colon);
        if (start != string::npos && start < line.find_first_of(",}", colon)) {
            size_t end = line.find("\"", start + 1);
            if (end != string::npos) {
                string val = line.substr(start + 1, end - start - 1);
                string decoded = "";
                bool escaped = false;
                for (char c : val) {
                    if (escaped) {
                        if (c == 'n') decoded += '\n';
                        else if (c == 't') decoded += '\t';
                        else if (c == '\"') decoded += '\"';
                        else if (c == '\\') decoded += '\\';
                        else decoded += c;
                        escaped = false;
                    } else if (c == '\\') {
                        escaped = true;
                    } else {
                        decoded += c;
                    }
                }
                return decoded;
            }
        } else {
            size_t i = colon + 1;
            while (i < line.length() && (line[i] == ' ' || line[i] == '\t' || line[i] == ':')) i++;
            string val = "";
            while (i < line.length() && line[i] != ',' && line[i] != '}' && line[i] != '\n' && line[i] != '\r') {
                val += line[i];
                i++;
            }
            return val;
        }
        return string("");
    };
    
    auto process_stream_data = [&](const string& chunk) {
        stream_buffer += chunk;
        size_t newline_pos;
        while ((newline_pos = stream_buffer.find('\n')) != string::npos) {
            string line = stream_buffer.substr(0, newline_pos);
            stream_buffer = stream_buffer.substr(newline_pos + 1);
            
            if (line.empty()) continue;
            
            string event = parse_json_field(line, "event");
            string data = parse_json_field(line, "data");
            
            if (event == "stdout") {
                res.stdout_output += data;
            } else if (event == "stderr") {
                res.stderr_output += data;
            } else if (event == "exit") {
                string ec_str = parse_json_field(line, "exit_code");
                if (!ec_str.empty()) res.exit_code = stoi(ec_str);
                string to_str = parse_json_field(line, "timed_out");
                if (to_str == "true") res.timed_out = true;
            }
            
            // Stream back to WebSocket clients in the room in real-time
            if (!ws_room_id.empty() && loop != nullptr) {
                string ws_payload = "{\"type\":\"output\",\"data\":" + line + "}";
                loop->defer([ws_room_id, ws_payload, aborted]() {
                    if (!*aborted) {
                        Session s;
                        if (session_manager.get_session(ws_room_id, s)) {
                            if (s.interviewer) {
                                auto* ws = (uWS::WebSocket<false, true, SocketData>*) s.interviewer;
                                ws->send(ws_payload, uWS::OpCode::TEXT);
                            }
                            if (s.candidate) {
                                auto* ws = (uWS::WebSocket<false, true, SocketData>*) s.candidate;
                                ws->send(ws_payload, uWS::OpCode::TEXT);
                            }
                        }
                    }
                });
            }
        }
    };
    
    while (true) {
        if (*aborted) {
            cout << "[Sidecar Client] Execution aborted by user. Closing socket." << endl;
            break;
        }
        
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            break;
        }
        buf[n] = '\0';
        
        if (!headers_passed) {
            response_accumulator.append(buf, n);
            size_t pos = response_accumulator.find("\r\n\r\n");
            if (pos != string::npos) {
                headers_passed = true;
                string remaining_body = response_accumulator.substr(pos + 4);
                process_stream_data(remaining_body);
            }
        } else {
            process_stream_data(string(buf, n));
        }
    }
    
    close(sock);
    return res;
}

int main() {
    if (!redis_client.connect("127.0.0.1", 6379)) {
        cerr << "Failed to connect to Redis server!" << endl;
        return 1;
    }
    cout << "Connected to Redis successfully!" << endl;

    // Start session expiry audit thread (scanning every 1 minute)
    thread expiry_thread([]() {
        while (true) {
            this_thread::sleep_for(chrono::minutes(1));
            // Expire sessions with no activity for 2 hours (120 minutes)
            vector<string> expired = session_manager.get_expired_sessions(2);
            for (const string& room_id : expired) {
                cout << "[Expiry Thread] Purging idle room: " << room_id << endl;
                thread_pool.enqueue([room_id]() {
                    redis_client.del("room:" + room_id + ":code");
                    redis_client.del("room:" + room_id + ":version");
                    redis_client.del("room:" + room_id + ":language");
                    redis_client.del("room:" + room_id + ":input");
                });
                session_manager.delete_session(room_id);
            }
        }
    });
    expiry_thread.detach();

    uWS::App()
        .get("/create", [](auto *res, auto *req) {
            string room_id = session_manager.create_session();
            res->writeHeader("Content-Type", "application/json");
            res->writeHeader("Access-Control-Allow-Origin", "*");
            res->end("{\"room_id\":\"" + room_id + "\"}");
        })
        .options("/*", [](auto *res, auto *req) {
            res->writeHeader("Access-Control-Allow-Origin", "*");
            res->writeHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            res->writeHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
            res->end();
        })
        .post("/run", [](auto *res, auto *req) {
            cout << "[/run] Request received" << endl;
            auto body = make_shared<string>();
            auto aborted = make_shared<bool>(false);
            res->onAborted([aborted]() {
                cout << "[/run] Request aborted by client" << endl;
                *aborted = true;
            });
            res->onData([res, body, aborted](string_view chunk, bool isLast) {
                body->append(chunk.data(), chunk.length());
                if (isLast) {
                    RunRequest run_req = parse_run_request(*body);
                    uWS::Loop* loop = uWS::Loop::get();
                    thread_pool.enqueue([res, run_req, aborted, loop]() {
                        ExecutionResult exec_res = run_code_via_sidecar(run_req.code, run_req.language, run_req.input, "", nullptr, aborted);
                        
                        string json_resp = "{"
                            "\"stdout\":\"" + escape_json(exec_res.stdout_output) + "\","
                            "\"stderr\":\"" + escape_json(exec_res.stderr_output) + "\","
                            "\"exit_code\":" + to_string(exec_res.exit_code) + ","
                            "\"timed_out\":" + (exec_res.timed_out ? "true" : "false") +
                        "}";
                        
                        loop->defer([res, json_resp, aborted]() {
                            if (!*aborted) {
                                res->writeHeader("Content-Type", "application/json");
                                res->writeHeader("Access-Control-Allow-Origin", "*");
                                res->end(json_resp);
                            }
                        });
                    });
                }
            });
        })
        .ws<SocketData>("/ws/:room_id/:role", {
            .upgrade = [](auto *res, auto *req, auto *context) {
                string room_id(req->getParameter("room_id"));
                string role(req->getParameter("role"));
                res->template upgrade<SocketData>(
                    {room_id, role, make_shared<bool>(false)},
                    req->getHeader("sec-websocket-key"),
                    req->getHeader("sec-websocket-protocol"),
                    req->getHeader("sec-websocket-extensions"),
                    context
                );
            },
            .open = [](auto *ws) {
                SocketData *data = ws->getUserData();
                if (!session_manager.join(data->room_id, ws, data->role)) {
                    ws->close();
                    return;
                }
                cout << "Client joined room " << data->room_id << " as " << data->role << endl;
                
                // Fetch state from Redis and send init message
                string code = redis_client.get("room:" + data->room_id + ":code");
                string ver_str = redis_client.get("room:" + data->room_id + ":version");
                string lang = redis_client.get("room:" + data->room_id + ":language");
                string problem = redis_client.get("room:" + data->room_id + ":problem");
                string input = redis_client.get("room:" + data->room_id + ":input");
                int version = ver_str.empty() ? 0 : stoi(ver_str);
                if (lang.empty()) lang = "cpp";
                
                // Initialize local session variables in memory
                session_manager.update_code_with_version(data->room_id, code, version);
                session_manager.update_language(data->room_id, lang);
                session_manager.update_problem(data->room_id, problem);
                session_manager.update_stdin(data->room_id, input);
                
                string init_resp = "{"
                    "\"type\":\"init\","
                    "\"code\":\"" + escape_json(code) + "\","
                    "\"version\":" + to_string(version) + ","
                    "\"language\":\"" + escape_json(lang) + "\","
                    "\"problem\":\"" + escape_json(problem) + "\","
                    "\"input\":\"" + escape_json(input) + "\""
                "}";
                ws->send(init_resp, uWS::OpCode::TEXT);
            },
            .message = [](auto *ws, string_view message, uWS::OpCode opCode) {
                SocketData *data = ws->getUserData();
                string text(message);
                WSMessage msg = parse_ws_message(text);
                
                if (msg.type == "edit") {
                    if (session_manager.update_code_with_version(data->room_id, msg.code, msg.version)) {
                        // Forward edit to peer
                        Session s;
                        if (session_manager.get_session(data->room_id, s)) {
                            void* other_ws = (data->role == "interviewer") ? s.candidate : s.interviewer;
                            if (other_ws) {
                                auto* peer = (uWS::WebSocket<false, true, SocketData>*) other_ws;
                                peer->send(text, opCode);
                            }
                        }
                        // Persist to Redis
                        thread_pool.enqueue([room_id = data->room_id, code = msg.code, version = msg.version]() {
                            redis_client.set("room:" + room_id + ":code", code);
                            redis_client.set("room:" + room_id + ":version", to_string(version));
                        });
                    }
                } else if (msg.type == "language") {
                    session_manager.update_language(data->room_id, msg.language);
                    // Forward language to peer
                    Session s;
                    if (session_manager.get_session(data->room_id, s)) {
                        void* other_ws = (data->role == "interviewer") ? s.candidate : s.interviewer;
                        if (other_ws) {
                            auto* peer = (uWS::WebSocket<false, true, SocketData>*) other_ws;
                            peer->send(text, opCode);
                        }
                    }
                    // Persist to Redis
                    thread_pool.enqueue([room_id = data->room_id, language = msg.language]() {
                        redis_client.set("room:" + room_id + ":language", language);
                    });
                } else if (msg.type == "problem") {
                    session_manager.update_problem(data->room_id, msg.problem);
                    // Forward problem to peer
                    Session s;
                    if (session_manager.get_session(data->room_id, s)) {
                        void* other_ws = (data->role == "interviewer") ? s.candidate : s.interviewer;
                        if (other_ws) {
                            auto* peer = (uWS::WebSocket<false, true, SocketData>*) other_ws;
                            peer->send(text, opCode);
                        }
                    }
                    // Persist to Redis
                    thread_pool.enqueue([room_id = data->room_id, problem = msg.problem]() {
                        redis_client.set("room:" + room_id + ":problem", problem);
                    });
                } else if (msg.type == "input") {
                    session_manager.update_stdin(data->room_id, msg.input);
                    // Forward input to peer
                    Session s;
                    if (session_manager.get_session(data->room_id, s)) {
                        void* other_ws = (data->role == "interviewer") ? s.candidate : s.interviewer;
                        if (other_ws) {
                            auto* peer = (uWS::WebSocket<false, true, SocketData>*) other_ws;
                            peer->send(text, opCode);
                        }
                    }
                    // Persist to Redis
                    thread_pool.enqueue([room_id = data->room_id, input = msg.input]() {
                        redis_client.set("room:" + room_id + ":input", input);
                    });
                } else if (msg.type == "run") {
                    Session s;
                    if (session_manager.get_session(data->room_id, s)) {
                        // Reset this socket's abort state before launch
                        *data->run_aborted = false;
                        
                        cout << "[WebSocket] Dispatching code run task. Room: " << data->room_id << endl;
                        uWS::Loop* loop = uWS::Loop::get();
                        thread_pool.enqueue([room_id = data->room_id, code = s.code, language = s.language, input = s.stdin_input, loop, aborted = data->run_aborted]() {
                            run_code_via_sidecar(code, language, input, room_id, loop, aborted);
                        });
                    }
                }
            },
            .close = [](auto *ws, int code, string_view message) {
                SocketData *data = ws->getUserData();
                cout << "Client left room " << data->room_id << " (" << data->role << ")" << endl;
                
                // Trigger mid-run cancellation if they were executing code
                *data->run_aborted = true;
                
                session_manager.leave(data->room_id, ws);
            }
        })
        .listen(4001, [](auto *listen_socket) {
            if (listen_socket) {
                cout << "CodePair Server running on port 4001" << endl;
            } else {
                cerr << "Failed to listen on port 4001" << endl;
            }
        })
        .run();

    return 0;
}
