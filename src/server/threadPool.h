#pragma once
#include <condition_variable>
#include <functional>
#include <queue>
#include <thread>

class ThreadPool {
public:
  using Task = std::function<void()>;

  ThreadPool(size_t threadCount);
  ~ThreadPool();
  void enqueue(Task task);

  ThreadPool(ThreadPool &other) = delete;
  ThreadPool(ThreadPool &&other) = delete;
  ThreadPool &operator=(ThreadPool &other) = delete;
  ThreadPool &operator=(ThreadPool &&other) = delete;

private:
  std::queue<Task> tasks;
  std::vector<std::thread> threads;
  std::mutex queueMutex;
  std::condition_variable condition;
  std::atomic<bool> running;
};