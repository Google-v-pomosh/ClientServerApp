#include "../inc/header.h"

ThreadPool::~ThreadPool() {
    terminate_pool_ = true;
    join();
}

void ThreadPool::setupThreadPool(uint32_t thread_count) {
    thread_pool_.clear();
    for(uint32_t i = 0; i < thread_count; ++i) {
        thread_pool_.emplace_back(&ThreadPool::workerLoop, this);
    }
}

void ThreadPool::workerLoop() {
    std::function<void()> work;
    while (!terminate_pool_) {
        {
            std::unique_lock lock(queue_mutex_);
            condition_variable_.wait(lock, [this]() { return !queue_work_.empty() || terminate_pool_; });
            if (terminate_pool_) {
                return;
            }
            work = queue_work_.front();
            queue_work_.pop();
        }
        work();
    }
}

void ThreadPool::join() {
    for(auto &thread : thread_pool_) {
       thread.join();
    }
}

uint32_t ThreadPool::getThreadCount() const {
    return thread_pool_.size();
}

void ThreadPool::restartJob() {
    terminate_pool_ = true;
    join();
    terminate_pool_ = false;
    std::queue<std::function<void()>> empty;
    std::swap(queue_work_, empty);
    setupThreadPool(thread_pool_.size());
}

void ThreadPool::stop() {
    terminate_pool_ = true;
    join();
}


