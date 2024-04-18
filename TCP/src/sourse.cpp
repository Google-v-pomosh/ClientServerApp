#include "../inc/header.h"

ThreadPool::~ThreadPool() {
    m_terminatePool_ = true;
    Join();
}

void ThreadPool::SetupThreadPool(uint32_t thread_count) {
    m_threadPool_.clear();
    for(uint32_t i = 0; i < thread_count; ++i) {
        m_threadPool_.emplace_back(&ThreadPool::WorkerLoop, this);
    }
}

void ThreadPool::WorkerLoop() {
    std::function<void()> work;
    while (!m_terminatePool_) {
        {
            std::unique_lock lock(m_queueMutex_);
            m_conditionVariable_.wait(lock, [this]() { return !m_queueWork_.empty() || m_terminatePool_; });
            if (m_terminatePool_) {
                return;
            }
            work = m_queueWork_.front();
            m_queueWork_.pop();
        }
        work();
    }
}

void ThreadPool::Join() {
    for(auto &thread : m_threadPool_) {
        thread.join();
    }
}

uint32_t ThreadPool::GetThreadCount() const {
    return m_threadPool_.size();
}

void ThreadPool::RestartJob() {
    m_terminatePool_ = true;
    Join();
    m_terminatePool_ = false;
    std::queue<std::function<void()>> empty;
    std::swap(m_queueWork_, empty);
    SetupThreadPool(m_threadPool_.size());
}

void ThreadPool::Stop() {
    m_terminatePool_ = true;
}

void ThreadPool::Start(uint32_t thread_count) {
    if (m_terminatePool_) {
        m_terminatePool_ = false;
        SetupThreadPool(thread_count);
    }
}