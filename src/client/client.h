#pragma once
#include "imsi.h"
#include "spdlog/logger.h"
#include <memory>
#include <netinet/in.h>
#include <optional>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>
#include <vector>

class Client {
public:
  struct ClientConfig {
    std::string serverIp;
    uint16_t serverPort;
    bool hasTimeout;
    uint32_t clientTimeoutSec;
    std::string logFileName;
    std::string logLevel;
    bool quiet;
  };
  void run(const IMSI &imsi);
  static std::unique_ptr<Client> fromConfigFile(const std::string &configFile);
  static std::unique_ptr<Client> fromConfig(const ClientConfig &config);
  static std::optional<ClientConfig>
  parseConfigFile(const std::string &configFile);

  ~Client();

protected:
  Client() = default;
  bool init(const ClientConfig &config);
  void deinit();
  void logEvent(const std::string &msg,
                spdlog::level::level_enum level = spdlog::level::debug);

  struct UdpSocketContext {
    int udpSocketFD;
    int epollFD;
    std::vector<unsigned char> recvBuffer;
  } udpSocketContext;

  ClientConfig config;

  std::shared_ptr<spdlog::logger> clientLogger;
};