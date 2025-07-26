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
  ~Server();
  Server(Server &other) = delete;
  Server &operator=(Server &other) = delete;
  Server(Server &&other) = delete;
  Server &operator=(Server &&other) = delete;

private:
  void sendUdpPacket(const std::string &response,
                     const sockaddr_in &clientAddr);
  static std::optional<ServerConfig>
  parseConfigFile(const std::string &configFile);
  bool init(const ServerConfig &config);
  void deinit();
  Server() = default;

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
  void logCDR(const CDREvent &cdrEvent);
  void logEvent(const std::string &msg,
                spdlog::level::level_enum level = spdlog::level::debug);

  std::map<IMSI, Session> sessions;
  std::mutex sessionMutex;

  void runEpollThread();
  void runHttpThread();
  void runCleanupThread();
  void
  addSession(IMSI imsi,
             std::chrono::time_point<std::chrono::steady_clock> expiration);

  void processUdpPacket(std::vector<unsigned char> packet,
                        const sockaddr_in &clientAddr);

  std::atomic<bool> running; // Main flag controlling the execution of threads
};