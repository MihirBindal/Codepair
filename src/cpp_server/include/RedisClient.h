#pragma once
#include <string>
#include <mutex>
#include <hiredis/hiredis.h>

using namespace std;

class RedisClient {
public:
    RedisClient();
    ~RedisClient();
    bool connect(string host = "127.0.0.1", int port = 6379);
    bool set(string key, string value);
    string get(string key);
    bool del(string key);
private:
    redisContext* context;
    mutex mtx;
};
