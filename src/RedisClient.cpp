#include "RedisClient.h"

RedisClient::RedisClient() : context(nullptr) {}

RedisClient::~RedisClient() {
    if (context) {
        redisFree(context);
    }
}

bool RedisClient::connect(string host, int port) {
    lock_guard<mutex> lock(mtx);
    if (context) {
        redisFree(context);
        context = nullptr;
    }
    context = redisConnect(host.c_str(), port);
    if (context == nullptr || context->err) {
        if (context) {
            redisFree(context);
            context = nullptr;
        }
        return false;
    }
    return true;
}

bool RedisClient::set(string key, string value) {
    lock_guard<mutex> lock(mtx);
    if (!context) return false;
    
    auto* reply = (redisReply*)redisCommand(context, "SET %s %s", key.c_str(), value.c_str());
    if (!reply) return false;
    
    freeReplyObject(reply);
    return true;
}

string RedisClient::get(string key) {
    lock_guard<mutex> lock(mtx);
    if (!context) return "";
    
    auto* reply = (redisReply*)redisCommand(context, "GET %s", key.c_str());
    if (!reply) return "";
    
    string result = "";
    if (reply->type == REDIS_REPLY_STRING) {
        result = reply->str;
    }
    
    freeReplyObject(reply);
    return result;
}
