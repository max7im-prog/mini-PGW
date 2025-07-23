#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <set>
#include <thread>
#include "imsi.h"

class Server {
public:
  struct ServerConfig {
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
  static std::optional<Server> fromConfigFile(const std::string &configFile);
  static std::optional<Server> fromConfig(const ServerConfig &config);
  Server(Server &&other) = default;
  Server &operator=(Server &&other) = default;

private:
  static std::optional<ServerConfig> parseConfigFile(const std::string &configFile);
  bool init(const ServerConfig &config);
  Server() = default;
  Server(Server &other) = delete;
  Server &operator=(Server &other) = delete;
  ServerConfig config;

  std::thread epollThread;
  std::thread httpThread;
  std::thread cleanupThread;
};