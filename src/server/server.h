#pragma once
#include "imsi.h"
#include "session.h"
#include "threadPool.h"
#include <cstdint>
#include <map>
#include <netinet/in.h>
#include <sys/socket.h>
#include <optional>
#include <set>
#include <string>
#include <thread>

class Server {
public:
  struct ServerConfig {
    size_t numUdpThreads;
    std::string ip;
    uint16_t udpPort;
    uint16_t httpPort;
    uint32_t sessionTimeoutSec;
    uint32_t gracefulShutdownTimeSec;
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
  void sendUdpPacket(const std::string& response,const sockaddr_in &clientAddr);
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

  std::map<IMSI, Session> sessions;
  std::mutex sessionMutex;

  void runEpollThread();
  void runHttpThread();
  void runCleanupThread();

  void processUdpPacket(std::vector<unsigned char> packet,
                        const sockaddr_in &clientAddr);

  std::atomic<bool>
      running; // Main flag controlling the execution of epoll thread
};