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

class Server {
public:
  struct ServerConfig {
    size_t numUdpThreads;
    std::string ip;
    uint16_t udpPort;
    uint16_t httpPort;
    uint32_t sessionTimeoutSec;
    uint32_t maxShutdownTimeSec;
    uint32_t preferredShutdownRateSessionsPerSec;
    std::string cdrFileName;
    std::string logFileName;
    std::string logLevel;
    std::set<IMSI> blacklist;
  };

  void run();
  static std::unique_ptr<Server> fromConfigFile(const std::string &configFile);
  static std::unique_ptr<Server> fromConfig(const ServerConfig &config);
  static std::optional<ServerConfig>
  parseConfigFile(const std::string &configFile);

  ~Server();
  Server(Server &other) = delete;
  Server &operator=(Server &other) = delete;
  Server(Server &&other) = delete;
  Server &operator=(Server &&other) = delete;

protected:
  Server() = default;

  virtual void sendUdpPacket(const std::string &response,
                     const sockaddr_in &clientAddr);
  
  bool init(const ServerConfig &config);
  void deinit();
  void logCDR(const CDREvent &cdrEvent);
  void logEvent(const std::string &msg,
                spdlog::level::level_enum level = spdlog::level::debug);
  void runEpollThread();
  void runHttpThread();
  void runCleanupThread();
  virtual void
  addSession(IMSI imsi,
             std::chrono::time_point<std::chrono::steady_clock> expiration);

  void processUdpPacket(std::vector<unsigned char> packet,
                        const sockaddr_in &clientAddr);

  ServerConfig config;

  std::thread epollThread;
  std::thread httpThread;
  std::thread cleanupThread;

  std::unique_ptr<ThreadPool> udpThreadPool;

  struct UdpSocketContext {
    int udpSocketFD;
    sockaddr_in udpAddr;
    int epollFD;
    std::vector<unsigned char> recvBuffer;
  } udpSocketContext;

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

  struct LoggingContext {
    std::shared_ptr<spdlog::logger> serverLogger;
    std::shared_ptr<spdlog::logger> cdrLogger;
  } loggingContext;

  std::map<IMSI, Session> sessions;
  std::mutex sessionMutex;
  std::atomic<bool> running; // Main flag controlling the execution of threads
};