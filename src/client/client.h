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
  /**
   * @brief Holds parameters for client configuration
   *
   */
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

  /**
   * @brief Creates a client instance from a JSON configuration file, returns
   * nullptr on failure
   *
   * @return std::unique_ptr<Client>
   */
  static std::unique_ptr<Client> fromConfigFile(const std::string &configFile);

  /**
   * @brief Creates a client instance from a config, returns nullptr on failure
   *
   * @return std::unique_ptr<Client>
   */
  static std::unique_ptr<Client> fromConfig(const ClientConfig &config);

  /**
   * @brief Creates a config from a json file, returns std::nullopt on failure
   *
   * @return std::optional<ClientConfig>
   */
  static std::optional<ClientConfig>
  parseConfigFile(const std::string &configFile);

  ~Client();

protected:
  Client() = default;

  /**
   * @brief Initializes the client from a config, returns false of failure
   */
  bool init(const ClientConfig &config);

  /**
   * @brief Deinitializes the client by closing socket and epollfd
   *
   */
  void deinit();

  /**
   * @brief Logs event into configured log file
   *
   * @param level Level of loggin from spdlog (ex. ERROR, DEBUG, INFO, etc.)
   */
  void logEvent(const std::string &msg,
                spdlog::level::level_enum level = spdlog::level::debug);

  /**
   * @brief Stores all data related to UDP socket(socketFD, epollFD, etc.)
   *
   */
  struct UdpSocketContext {
    int udpSocketFD;
    int epollFD;
    std::vector<unsigned char> recvBuffer;
  } udpSocketContext;

  ClientConfig config;

  std::shared_ptr<spdlog::logger> clientLogger;
};