#include "client.h"
#include "spdlog/common.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <sys/epoll.h>
std::optional<Client::ClientConfig>
Client::parseConfigFile(const std::string &configFile) {
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

  ClientConfig clientConfig;

  if (!data.contains("serverIp") || !data["serverIp"].is_string())
    return std::nullopt;
  clientConfig.serverIp = data["serverIp"].get<std::string>();
  struct in_addr addr;
  if (inet_pton(AF_INET, clientConfig.serverIp.c_str(), &addr) != 1)
    return std::nullopt;

  if (!data.contains("serverPort") || !data["serverPort"].is_number_unsigned())
    return std::nullopt;
  clientConfig.serverPort = data["serverPort"].get<uint16_t>();

  if (!data.contains("hasTimeout") || !data["hasTimeout"].is_boolean())
    return std::nullopt;
  clientConfig.hasTimeout = data["hasTimeout"].get<bool>();

  if (!data.contains("clientTimeoutSec") ||
      !data["clientTimeoutSec"].is_number_unsigned())
    return std::nullopt;
  clientConfig.clientTimeoutSec = data["clientTimeoutSec"].get<uint32_t>();

  if (!data.contains("logFileName") || !data["logFileName"].is_string())
    return std::nullopt;
  clientConfig.logFileName = data["logFileName"].get<std::string>();

  if (!data.contains("logLevel") || !data["logLevel"].is_string())
    return std::nullopt;
  clientConfig.logLevel = data["logLevel"].get<std::string>();

  if (!data.contains("quiet") || !data["quiet"].is_boolean())
    return std::nullopt;
  clientConfig.quiet = data["quiet"].get<bool>();

  return clientConfig;
}

std::unique_ptr<Client> Client::fromConfigFile(const std::string &configFile) {
  auto config = parseConfigFile(configFile);
  if (!config.has_value()) {
    return nullptr;
  }
  return Client::fromConfig(config.value());
}

std::unique_ptr<Client> Client::fromConfig(const ClientConfig &config) {
  std::unique_ptr<Client> client = std::unique_ptr<Client>(new Client);
  if (!client->init(config)) {
    return nullptr;
  }
  return client;
}

bool Client::init(const ClientConfig &config) {
  // ---- Setup Logger ----
  clientLogger = spdlog::rotating_logger_mt("clientLogger", config.logFileName,
                                            1048576 * 5, 3);
  if (!clientLogger) {
    std::cerr << "Failed to create logger: " << config.logFileName << std::endl;
    return false;
  }

  clientLogger->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");

  if (config.logLevel == "INFO") {
    clientLogger->set_level(spdlog::level::info);
  } else if (config.logLevel == "CRIT") {
    clientLogger->set_level(spdlog::level::critical);
  } else if (config.logLevel == "DEBUG") {
    clientLogger->set_level(spdlog::level::debug);
  } else if (config.logLevel == "ERROR") {
    clientLogger->set_level(spdlog::level::err);
  } else if (config.logLevel == "WARN") {
    clientLogger->set_level(spdlog::level::warn);
  } else if (config.logLevel == "TRACE") {
    clientLogger->set_level(spdlog::level::trace);
  } else if (config.logLevel == "OFF") {
    clientLogger->set_level(spdlog::level::off);
  }
  spdlog::flush_every(std::chrono::seconds(1));

  udpSocketContext.udpSocketFD = socket(AF_INET, SOCK_DGRAM, 0);
  if (udpSocketContext.udpSocketFD < 0) {
    logEvent("Socket error: " + std::string(strerror(errno)),
             spdlog::level::err);
    return false;
  }

  int flags = fcntl(udpSocketContext.udpSocketFD, F_GETFL, 0);
  if (flags == -1)
    flags = 0;
  if (fcntl(udpSocketContext.udpSocketFD, F_SETFL, flags | O_NONBLOCK) < 0) {
    logEvent("Failed to set non-blocking mode: " + std::string(strerror(errno)),
             spdlog::level::err);
    close(udpSocketContext.udpSocketFD);
    return false;
  }

  udpSocketContext.recvBuffer.resize(2048);
  udpSocketContext.epollFD = epoll_create1(0);
  if (udpSocketContext.epollFD < 0) {
    logEvent("Epoll_create1 error: " + std::string(strerror(errno)),
             spdlog::level::err);
    close(udpSocketContext.udpSocketFD);
    return false;
  }

  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = udpSocketContext.udpSocketFD;
  if (epoll_ctl(udpSocketContext.epollFD, EPOLL_CTL_ADD,
                udpSocketContext.udpSocketFD, &ev) < 0) {
    logEvent("Epoll_ctl error: " + std::string(strerror(errno)),
             spdlog::level::err);
    close(udpSocketContext.udpSocketFD);
    close(udpSocketContext.epollFD);
    return false;
  }

  this->config = config;

  return true;
}

void Client::deinit() {
  if (udpSocketContext.udpSocketFD != -1) {
    close(udpSocketContext.udpSocketFD);
    udpSocketContext.udpSocketFD = -1;
  }
  if (udpSocketContext.epollFD != -1) {
    close(udpSocketContext.epollFD);
    udpSocketContext.epollFD = -1;
  }
}

Client::~Client() { deinit(); }

void Client::logEvent(const std::string &msg, spdlog::level::level_enum level) {
  clientLogger->log(level, msg);
}

void Client::run(const IMSI &imsi) {
  constexpr size_t MAX_EVENTS = 1;
  auto bcdBytes = imsi.toBCDBytes();
  sockaddr_in serverAddr{};

  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(config.serverPort);
  if (inet_pton(AF_INET, config.serverIp.c_str(), &serverAddr.sin_addr) <= 0) {
    std::ostringstream oss;
    oss << "Invalid address: " << config.serverIp;
    logEvent(oss.str(), spdlog::level::err);
    return;
  }

  // Send IMSI bytes
  ssize_t sent =
      sendto(udpSocketContext.udpSocketFD, bcdBytes.data(), bcdBytes.size(), 0,
             (sockaddr *)&serverAddr, sizeof(serverAddr));
  if (sent < 0) {
    std::ostringstream oss;
    oss << "Sendto error: " << strerror(errno);
    logEvent(oss.str(), spdlog::level::err);
    return;
  }
  {
    std::ostringstream oss;
    oss << "Sent IMSI " << imsi.toStdString() << " to " << config.serverIp
        << ":" << config.serverPort;
    logEvent(oss.str(), spdlog::level::info);
  }

  // Receive response
  epoll_event events[MAX_EVENTS];
  int32_t timeout = -1;
  if (config.hasTimeout) {
    timeout = config.clientTimeoutSec * 1000;
  }
  int n = epoll_wait(udpSocketContext.epollFD, events, MAX_EVENTS, timeout);
  sockaddr_in fromAddr{};
  socklen_t fromLen = sizeof(fromAddr);
  if (n > 0) {
    for (int i = 0; i < n; i++) {
      auto event = events[i];
      auto recvSize =
          recvfrom(event.data.fd, udpSocketContext.recvBuffer.data(),
                   udpSocketContext.recvBuffer.size(), 0,
                   reinterpret_cast<sockaddr *>(&fromAddr), &fromLen);

      if (recvSize < 0) {
        std::ostringstream oss;
        oss << "Recvfrom error: " << strerror(errno);
        logEvent(oss.str(), spdlog::level::err);
        return;
      }
      size_t pos = (recvSize < 0) ? 0 : static_cast<size_t>(recvSize);
      udpSocketContext
          .recvBuffer[std::min(pos, udpSocketContext.recvBuffer.size() - 1)] =
          '\0';
      {
        std::ostringstream oss;
        oss << "Response from " << inet_ntoa(fromAddr.sin_addr) << ":"
            << ntohs(fromAddr.sin_port) << ": \""
            << udpSocketContext.recvBuffer.data() << "\"";
        logEvent(oss.str(), spdlog::level::info);
      }
      if (!config.quiet) {
        std::cout << "Response: " << udpSocketContext.recvBuffer.data()
                  << std::endl;
      }
    }
  } else if (n == 0) {
    logEvent("Timed out", spdlog::level::info);
    if (!config.quiet) {
      std::cout << "Timed out" << std::endl;
    }
  }
}