#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

template<typename T>
class ThreadSafeQueue {
public:
  ThreadSafeQueue() = default;
  ~ThreadSafeQueue() = default;

  void push(T item) {
    {
      std::lock_guard<std::mutex> lk(m_);
      q_.push(std::move(item));
    }
    cv_.notify_one();
  }

  std::optional<T> pop_wait(std::atomic<bool>& stopFlag) {
    std::unique_lock<std::mutex> lk(m_);
    cv_.wait(lk, [&]{ return stopFlag.load() || !q_.empty(); });
    if (stopFlag.load() && q_.empty()) return std::nullopt;
    T it = std::move(q_.front());
    q_.pop();
    return it;
  }size_t size() const {
    std::lock_guard<std::mutex> lk(m_);
    return q_.size();
  }

private:
  mutable std::mutex m_;
  std::condition_variable cv_;
  std::queue<T> q_;
};EOF
