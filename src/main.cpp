#include <App.h>
#include <iostream>
#include <string_view>
#include <string>
#include <memory>
#include "SessionManager.h"
#include "RedisClient.h"
#include "ThreadPool.h"
#include "CodeExecutor.h"

using namespace std;

struct SocketData {
    string room_id;
    string role;
};

struct RunRequest {
    string code;
    string language;
};

SessionManager session_manager;
RedisClient redis_client;
ThreadPool thread_pool(4);

RunRequest parse_run_request(const string& body) {
    RunRequest req;
    size_t lang_pos = body.find("\"language\"");
    if (lang_pos != string::npos) {
        size_t start = body.find("\"", lang_pos + 10);
        if (start != string::npos) {
            size_t end = body.find("\"", start + 1);
            if (end != string::npos) {
                req.language = body.substr(start + 1, end - start - 1);
            }
        }
    }
    size_t code_pos = body.find("\"code\"");
    if (code_pos != string::npos) {
        size_t start = body.find("\"", code_pos + 6);
        if (start != string::npos) {
            string code_val = "";
            bool escaped = false;
            size_t i = start + 1;
            while (i < body.length()) {
                char c = body[i];
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
            req.code = code_val;
        }
    }
    return req;
}

int main() {
    if (!redis_client.connect("127.0.0.1", 6379)) {
        cerr << "Failed to connect to Redis server!" << endl;
        return 1;
    }
    cout << "Connected to Redis successfully!" << endl;

    uWS::App()
        .get("/create", [](auto *res, auto *req) {
            string room_id = session_manager.create_session();
            res->writeHeader("Content-Type", "application/json");
            res->writeHeader("Access-Control-Allow-Origin", "*");
            res->end("{\"room_id\":\"" + room_id + "\"}");
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
                    cout << "[/run] Finished reading body. Size: " << body->size() << " bytes." << endl;
                    RunRequest run_req = parse_run_request(*body);
                    cout << "[/run] Enqueuing execution task to ThreadPool. Language: " << run_req.language << endl;
                    uWS::Loop* loop = uWS::Loop::get();
                    thread_pool.enqueue([res, run_req, aborted, loop]() {
                        cout << "[ThreadPool] Task started. Spawning sandbox..." << endl;
                        CodeExecutor executor;
                        ExecutionResult exec_res = executor.execute(run_req.code, run_req.language);
                        cout << "[ThreadPool] Sandbox execution completed. Exit code: " << exec_res.exit_code << endl;
                        
                        auto escape_json = [](const string& s) {
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
                        };
                        
                        string json_resp = "{"
                            "\"stdout\":\"" + escape_json(exec_res.stdout_output) + "\","
                            "\"stderr\":\"" + escape_json(exec_res.stderr_output) + "\","
                            "\"exit_code\":" + to_string(exec_res.exit_code) + ","
                            "\"timed_out\":" + (exec_res.timed_out ? "true" : "false") +
                        "}";
                        
                        cout << "[ThreadPool] Deferring response back to main loop..." << endl;
                        loop->defer([res, json_resp, aborted]() {
                            cout << "[Main Thread] Defer callback triggered. Aborted: " << (*aborted ? "yes" : "no") << endl;
                            if (!*aborted) {
                                res->writeHeader("Content-Type", "application/json");
                                res->writeHeader("Access-Control-Allow-Origin", "*");
                                res->end(json_resp);
                                cout << "[Main Thread] Response sent successfully" << endl;
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
                    {room_id, role},
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
                
                string code = redis_client.get("room:" + data->room_id + ":code");
                if (!code.empty()) {
                    ws->send(code, uWS::OpCode::TEXT);
                }
            },
            .message = [](auto *ws, string_view message, uWS::OpCode opCode) {
                SocketData *data = ws->getUserData();
                
                Session s;
                if (session_manager.get_session(data->room_id, s)) {
                    void* other_ws = (data->role == "interviewer") ? s.candidate : s.interviewer;
                    if (other_ws) {
                        auto* peer = (uWS::WebSocket<false, true, SocketData>*) other_ws;
                        peer->send(message, opCode);
                    }
                }
                
                session_manager.update_code(data->room_id, string(message));
                
                thread_pool.enqueue([room_id = data->room_id, code = string(message)]() {
                    redis_client.set("room:" + room_id + ":code", code);
                });
            },
            .close = [](auto *ws, int code, string_view message) {
                SocketData *data = ws->getUserData();
                cout << "Client left room " << data->room_id << " (" << data->role << ")" << endl;
                session_manager.leave(data->room_id, ws);
            }
        })
        .listen(9001, [](auto *listen_socket) {
            if (listen_socket) {
                cout << "CodePair Server running on port 9001" << endl;
            } else {
                cerr << "Failed to listen on port 9001" << endl;
            }
        })
        .run();

    return 0;
}
