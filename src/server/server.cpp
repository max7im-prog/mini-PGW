#include "server.h"
#include "nlohmann/json_fwd.hpp"
#include <arpa/inet.h>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <sys/epoll.h>

std::optional<Server::ServerConfig>
Server::parseConfigFile(const std::string &configFile) {
  std::ifstream jsonFile(configFile);
  if (!jsonFile.is_open()) {
    return std::nullopt;
  }

  nlohmann::json data;
  try {
    data = nlohmann::json::parse(jsonFile);
  } catch (const nlohmann::json::parse_error &e) {
    return std::nullopt;
  }

  ServerConfig serverConfig;

  if (!data.contains("numUdpThreads") ||
      !data["numUdpThreads"].is_number_unsigned())
    return std::nullopt;
  serverConfig.numUdpThreads = data["numUdpThreads"].get<size_t>();

  if (!data.contains("ip") || !data["ip"].is_string())
    return std::nullopt;
  serverConfig.ip = data["ip"].get<std::string>();

  if (!data.contains("cdrFileName") || !data["cdrFileName"].is_string())
    return std::nullopt;
  serverConfig.cdrFileName = data["cdrFileName"].get<std::string>();

  if (!data.contains("logFileName") || !data["logFileName"].is_string())
    return std::nullopt;
  serverConfig.logFileName = data["logFileName"].get<std::string>();

  if (!data.contains("logLevel") || !data["logLevel"].is_string())
    return std::nullopt;
  serverConfig.logLevel = data["logLevel"].get<std::string>();

  if (!data.contains("udpPort") || !data["udpPort"].is_number_unsigned())
    return std::nullopt;
  serverConfig.udpPort = data["udpPort"].get<uint16_t>();

  if (!data.contains("httpPort") || !data["httpPort"].is_number_unsigned())
    return std::nullopt;
  serverConfig.httpPort = data["httpPort"].get<uint16_t>();

  if (!data.contains("sessionTimeoutSec") ||
      !data["sessionTimeoutSec"].is_number_unsigned())
    return std::nullopt;
  serverConfig.sessionTimeoutSec = data["sessionTimeoutSec"].get<uint32_t>();

  if (!data.contains("gracefulShutdownTimeSec") ||
      !data["gracefulShutdownTimeSec"].is_number_unsigned())
    return std::nullopt;
  serverConfig.gracefulShutdownTimeSec =
      data["gracefulShutdownTimeSec"].get<uint32_t>();

  // Blacklist
  if (data.contains("blacklist")) {
    if (!data["blacklist"].is_array())
      return std::nullopt;
    for (const auto &imsiStr : data["blacklist"]) {
      if (!imsiStr.is_string())
        return std::nullopt;
      auto imsiOpt = IMSI::fromStdString(imsiStr.get<std::string>());
      if (!imsiOpt)
        return std::nullopt;
      serverConfig.blacklist.insert(*imsiOpt);
    }
  } else {
    serverConfig.blacklist = {};
  }

  return serverConfig;
}

std::unique_ptr<Server> Server::fromConfigFile(const std::string &configFile) {
  auto config = Server::parseConfigFile(configFile);
  if (!config.has_value()) {
    return nullptr;
  }
  return Server::fromConfig(config.value());
}

std::unique_ptr<Server> Server::fromConfig(const ServerConfig &config) {
  std::unique_ptr<Server> server = std::unique_ptr<Server>(new Server);
  if (!server->init(config)) {
    return nullptr;
  }
  return server;
}

bool Server::init(const ServerConfig &config) {
  if (config.numUdpThreads == 0) {
    return false;
  }

  this->config = config;

  udpThreadPool = ThreadPool::create(config.numUdpThreads);
  if (!udpThreadPool) {
    return false;
  }

  udpSocket.udpSocketFD = socket(AF_INET, SOCK_DGRAM, 0);
  if (udpSocket.udpSocketFD < 0) {
    return false;
  }

  udpSocket.udpAddr = {};
  udpSocket.udpAddr.sin_family = AF_INET;
  udpSocket.udpAddr.sin_port = htons(config.udpPort);
  if (inet_pton(AF_INET, config.ip.c_str(), &udpSocket.udpAddr.sin_addr) != 1) {
    return false;
  }

  if (bind(udpSocket.udpSocketFD, (sockaddr *)&udpSocket.udpAddr,
           sizeof(udpSocket.udpAddr)) < 0) {
    return false;
  }

  // Set non-blocking
  int flags = fcntl(udpSocket.udpSocketFD, F_GETFL, 0);
  if (flags < 0 ||
      fcntl(udpSocket.udpSocketFD, F_SETFL, flags | O_NONBLOCK) < 0) {
    return false;
  }

  udpSocket.epollFD = epoll_create1(0);
  if (udpSocket.epollFD < 0) {
    return false;
  }

  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = udpSocket.udpSocketFD;
  if (epoll_ctl(udpSocket.epollFD, EPOLL_CTL_ADD, udpSocket.udpSocketFD, &ev) <
      0) {
    return false;
  }

  udpSocket.recvBuffer.resize(2048); // Adjust size as needed

  return true;
}
