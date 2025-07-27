#pragma once
#include "CDREvent.h"
#include "imsi.h"
#include "session.h"
#include "spdlog/logger.h"
#include "threadPool.h"
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <httplib.h>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <optional>
#include <queue>
#include <set>
#include <spdlog/sinks/rotating_file_sink.h>
#include <string>
#include <sys/socket.h>
#include <thread>

/**
 * @class Server
 * @brief Handles UDP and HTTP server logic.
 *
 * The Server class is responsible for managing sessions, processing UDP
 * packets, and providing HTTP endpoints for monitoring and control.
 */
class Server {
public:
  /**
   * @class ServerConfig
   * @brief Holds parameters for server configuration
   *
   */
  struct ServerConfig {
    size_t numUdpThreads;
    std::string ip;
    uint16_t udpPort;
    uint16_t httpPort;
    uint32_t sessionTimeoutSec;
    uint32_t maxShutdownTimeSec;
    uint32_t preferredShutdownRateSessionsPerSec;
    std::string cdrFileName; // File to log CDR (Call Detail Record) events
    std::string logFileName;
    std::string logLevel;
    std::set<IMSI> blacklist;
  };

  /**
   * @brief Runs the server
   *
   *
   * This method starts epoll thread, http thread and cleanup thread and then
   * waits for them to finish
   */
  void run();

  /**
   * @brief Creates a server instance from a JSON configuration file, returns
   * nullptr on failure
   *
   * @return std::unique_ptr<Server>
   */
  static std::unique_ptr<Server> fromConfigFile(const std::string &configFile);

  /**
   * @brief Creates a server instance from a config, returns nullptr on failure
   *
   * @return std::unique_ptr<Server>
   */
  static std::unique_ptr<Server> fromConfig(const ServerConfig &config);

  /**
   * @brief Creates a config from a json file, returns std::nullopt on failure
   *
   * @return std::optional<ServerConfig>
   */
  static std::optional<ServerConfig>
  parseConfigFile(const std::string &configFile);

  ~Server();
  Server(Server &other) = delete;
  Server &operator=(Server &other) = delete;
  Server(Server &&other) = delete;
  Server &operator=(Server &&other) = delete;

protected:
  Server() = default;

  /**
   * @brief Sends udp packet to ip address from a socket which server is
   * configured to use
   *
   */
  virtual void sendUdpPacket(const std::string &response,
                             const sockaddr_in &clientAddr);

  /**
   * @brief Initializes the server from a config, returns false of failure
   */
  bool init(const ServerConfig &config);

  /**
   * @brief Deinitializes the server by closing sockets, epollfds and resetting
   * a thread pool
   *
   */
  void deinit();

  /**
   * @brief Logs a CDR (Call Detail Record) event into a configured CDR log file
   *
   */
  void logCDR(const CDREvent &cdrEvent);

  /**
   * @brief Logs event into configured log file
   *
   * @param level Level of loggin from spdlog (ex. ERROR, DEBUG, INFO, etc.)
   */
  void logEvent(const std::string &msg,
                spdlog::level::level_enum level = spdlog::level::debug);

  /**
   * @brief Manages incloming UDP packets until the 'running' is set to false
   *
   * Manages incoming udp packets by using epoll_wait() continuously until the
   * 'running' flag is set to false. For each packet that requires a response,
   * enqueues a task into a thread pool.
   *
   */
  void runEpollThread();

  /**
   * @brief Manages http connections
   *
   * Manages http connections with help of cpp-httplib, supports /stop and
   * /check_subscribers endpoints.
   */
  void runHttpThread();

  /**
   * @brief Manages cleanup of sessions.
   *
   * Manages cleanup of sessions by storing them in a priority queue sorted by
   * expiration time. Also handles graceful offload after the 'running' flag is
   * set to false.
   */
  void runCleanupThread();

  /**
   * @brief Registers a session by adding it to a list of sessions and
   * expiration queue
   *
   */
  virtual void
  addSession(IMSI imsi,
             std::chrono::time_point<std::chrono::steady_clock> expiration);

  /**
   * @brief Implements the logic of handling the packet
   *
   * Implements the logic of handling the packet - either creates a new session,
   * prolongs an existing one or rejects the IMSI. Sends a response via UDP if
   * needed.
   */
  virtual void processUdpPacket(std::vector<unsigned char> packet,
                                const sockaddr_in &clientAddr);

  ServerConfig config;

  std::thread epollThread;
  std::thread httpThread;
  std::thread cleanupThread;

  std::unique_ptr<ThreadPool> udpThreadPool;

  /**
   * @brief Stores all data related to UDP socket(socketFD, epollFD, etc.)
   *
   */
  struct UdpSocketContext {
    int udpSocketFD;
    sockaddr_in udpAddr;
    int epollFD;
    std::vector<unsigned char> recvBuffer;
  } udpSocketContext;

  /**
   * @brief Stores all data related to cleanup
   *
   */
  struct CleanupContext {
    struct ExpirationEntry {
      std::chrono::steady_clock::time_point expiration;
      IMSI imsi;
      bool operator>(const ExpirationEntry &other) const {
        return expiration > other.expiration;
      }
    };
    std::priority_queue<ExpirationEntry, std::vector<ExpirationEntry>,
                        std::greater<>>
        cleanupQueue;
    std::condition_variable cleanupCV;
  } cleanupContext;

  /**
   * @brief Stores all data related to logging
   *
   */
  struct LoggingContext {
    std::shared_ptr<spdlog::logger> serverLogger;
    std::shared_ptr<spdlog::logger> cdrLogger;
  } loggingContext;

  std::map<IMSI, Session> sessions;
  std::mutex sessionMutex;
  std::atomic<bool> running; // Main flag controlling the execution of threads
};