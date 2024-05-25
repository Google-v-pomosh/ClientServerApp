#include "../inc/header.h"

NetworkThreadPool::~NetworkThreadPool() {
    m_terminatePool_ = true;
    JoinThreads();
}

void NetworkThreadPool::ConfigureThreadPool(uint32_t thread_count) {
    m_threadPool_.clear();
    for(uint32_t i = 0; i < thread_count; ++i) {
        m_threadPool_.emplace_back(&NetworkThreadPool::ThreadWorkerLoop, this);
    }
}

//TODO
void NetworkThreadPool::ThreadWorkerLoop() {
    while (!m_terminatePool_) {
        std::function<void()> work;
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

void NetworkThreadPool::JoinThreads() {
    for(auto &thread : m_threadPool_) {
        thread.join();
    }
}

uint32_t NetworkThreadPool::GetThreadCount() const {
    return m_threadPool_.size();
}

void NetworkThreadPool::ResetJob() {
    m_terminatePool_ = true;
    JoinThreads();
    m_terminatePool_ = false;
    std::queue<std::function<void()>> empty;
    std::swap(m_queueWork_, empty);
    ConfigureThreadPool(m_threadPool_.size());
}

void NetworkThreadPool::StopThreads() {
    m_terminatePool_ = true;
}

void NetworkThreadPool::StartThreads(uint32_t thread_count) {
    if (m_terminatePool_) {
        m_terminatePool_ = false;
        ConfigureThreadPool(thread_count);
    }
}

std::string NetworkThreadPool::ExtractString(DataBuffer_t::const_iterator &it) {
    std::string result;
    while (*it != '\0') {
        result += *it;
        ++it;
    }
    ++it;
    return result;
}

void NetworkThreadPool::AppendString(DataBuffer_t &buffer, std::string_view str) {
    Append<uint64_t>(buffer, str.size());
    for(const char& ch : str)
        buffer.push_back(reinterpret_cast<const uint8_t&>(ch));
}
