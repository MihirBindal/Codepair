#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <type_traits>

using namespace std;

class ThreadPool {
public:
    ThreadPool(size_t threads);
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> future<typename invoke_result<F, Args...>::type>;
    ~ThreadPool();
private:
    vector<thread> workers;
    queue<function<void()>> tasks;
    mutex queue_mutex;
    condition_variable condition;
    bool stop;
};

template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> future<typename invoke_result<F, Args...>::type> 
{
    using return_type = typename invoke_result<F, Args...>::type;
    auto task = make_shared<packaged_task<return_type()>>(
        bind(forward<F>(f), forward<Args>(args)...)
    );
    future<return_type> res = task->get_future();
    {
        unique_lock<mutex> lock(queue_mutex);
        if(stop) {
            throw runtime_error("enqueue on stopped ThreadPool");
        }
        tasks.emplace([task]() { (*task)(); });
    }
    condition.notify_one();
    return res;
}
