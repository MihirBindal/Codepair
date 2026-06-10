#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t threads) : stop(false) {
    for(size_t i = 0; i < threads; ++i) {
        workers.emplace_back([this] {
            while(true) {
                function<void()> task;
                {
                    unique_lock<mutex> lock(this->queue_mutex);
                    this->condition.wait(lock, [this] {
                        return this->stop || !this->tasks.empty();
                    });
                    if(this->stop && this->tasks.empty()) {
                        return;
                    }
                    task = move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        unique_lock<mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for(thread &worker: workers) {
        if(worker.joinable()) {
            worker.join();
        }
    }
}
