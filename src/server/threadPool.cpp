#include "threadPool.h"
#include <mutex>

ThreadPool::ThreadPool(size_t threadCount) : running(true) {
  for (size_t i = 0; i < threadCount; i++) {
    threads.emplace_back([this]() {
      while (running) {
        Task task;
        {
          std::unique_lock<std::mutex> lock(queueMutex);
          condition.wait(lock, [this]() { return !tasks.empty() || !running; });
          if (!running && tasks.empty()) {
            return;
          }
          task = std::move(tasks.front());
          tasks.pop();
        }
        task();
      }
    });
  }
};

ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queueMutex);
    running = false;
  }
  condition.notify_all();
  for (auto &t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }
}

void ThreadPool::enqueue(Task task) {
  {
    std::unique_lock<std::mutex> lock(queueMutex);
    tasks.push(std::move(task));
  }
  condition.notify_one();
}