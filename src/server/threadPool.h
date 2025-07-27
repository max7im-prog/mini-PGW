#pragma once
#include <condition_variable>
#include <functional>
#include <queue>
#include <thread>

/**
 * @class ThreadPool
 * @brief Class to handle parallel execution of functions.
 *
 */
class ThreadPool {
public:
  using Task = std::function<void()>;

  /**
   * @brief Creates a new instance of a ThreadPool, returns nullptr on failure
   *
   * @return std::unique_ptr<ThreadPool>
   */
  static std::unique_ptr<ThreadPool> create(size_t threadCount);

  ~ThreadPool();

  /**
   * @brief Enqueues a task for execution
   *
   * @param task A callable object representing the task to be executed.
   *             The task must be thread-safe if it accesses shared resources.
   */
  void enqueue(Task task);

  ThreadPool(ThreadPool &other) = delete;
  ThreadPool &operator=(ThreadPool &other) = delete;
  ThreadPool(ThreadPool &&other) = delete;
  ThreadPool &operator=(ThreadPool &&other) = delete;

protected:
  explicit ThreadPool(size_t threadCount);
  std::queue<Task> tasks;
  std::vector<std::thread> threads;
  std::mutex queueMutex;
  std::condition_variable condition;
  std::atomic<bool> running;
};