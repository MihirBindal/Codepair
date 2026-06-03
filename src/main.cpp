#include <App.h>
#include <iostream>
#include <string_view>
#include <string>
#include "SessionManager.h"
#include "RedisClient.h"
#include "ThreadPool.h"

using namespace std;

struct SocketData {
    string room_id;
    string role;
};

SessionManager session_manager;
RedisClient redis_client;
ThreadPool thread_pool(4);

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
