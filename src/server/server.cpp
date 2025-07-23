#include "server.h"
#include "nlohmann/json_fwd.hpp"
#include <arpa/inet.h>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <sys/epoll.h>
#include <sys/socket.h>

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

  udpSocketContext.udpSocketFD = socket(AF_INET, SOCK_DGRAM, 0);
  if (udpSocketContext.udpSocketFD < 0) {
    return false;
  }

  udpSocketContext.udpAddr = {};
  udpSocketContext.udpAddr.sin_family = AF_INET;
  udpSocketContext.udpAddr.sin_port = htons(config.udpPort);
  if (inet_pton(AF_INET, config.ip.c_str(),
                &udpSocketContext.udpAddr.sin_addr) != 1) {
    return false;
  }

  if (bind(udpSocketContext.udpSocketFD, (sockaddr *)&udpSocketContext.udpAddr,
           sizeof(udpSocketContext.udpAddr)) < 0) {
    return false;
  }

  // Set non-blocking
  int flags = fcntl(udpSocketContext.udpSocketFD, F_GETFL, 0);
  if (flags < 0 ||
      fcntl(udpSocketContext.udpSocketFD, F_SETFL, flags | O_NONBLOCK) < 0) {
    return false;
  }

  udpSocketContext.epollFD = epoll_create1(0);
  if (udpSocketContext.epollFD < 0) {
    return false;
  }

  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = udpSocketContext.udpSocketFD;
  if (epoll_ctl(udpSocketContext.epollFD, EPOLL_CTL_ADD,
                udpSocketContext.udpSocketFD, &ev) < 0) {
    return false;
  }

  udpSocketContext.recvBuffer.resize(2048); // Adjust size as needed

  return true;
}

void Server::deinit() {
  if (udpSocketContext.udpSocketFD != -1) {
    close(udpSocketContext.udpSocketFD);
    udpSocketContext.udpSocketFD = -1;
  }

  if (udpSocketContext.epollFD != -1) {
    close(udpSocketContext.epollFD);
    udpSocketContext.epollFD = -1;
  }

  udpThreadPool.reset();

  {
    std::lock_guard<std::mutex> lock(sessionMutex);
    sessions.clear();
  }
}

Server::~Server() { deinit(); }

void Server::runEpollThread() {
  constexpr size_t MAX_EVENTS = 1024;
  constexpr int EPOLL_TIMEOUT_MSEC = 500;
  epoll_event events[MAX_EVENTS];

  // Main loop 
  while (this->running) {
    int numEvents = epoll_wait(udpSocketContext.epollFD, events, MAX_EVENTS,
                               EPOLL_TIMEOUT_MSEC);
    if (numEvents > 0) {
      for (int i = 0; i < numEvents; i++) {
        if ((events[i].events & EPOLLIN) &&
            events[i].data.fd == udpSocketContext.udpSocketFD) {
          sockaddr_in clientAddr{};
          socklen_t addrLen = sizeof(clientAddr);
          ssize_t recvLen = recvfrom(
              udpSocketContext.udpSocketFD, udpSocketContext.recvBuffer.data(),
              udpSocketContext.recvBuffer.size(), 0,
              reinterpret_cast<sockaddr *>(&clientAddr), &addrLen);
          if (recvLen > 0) {
            std::vector<char> packet{udpSocketContext.recvBuffer.begin(),
                                     udpSocketContext.recvBuffer.begin() +
                                         recvLen};
            this->udpThreadPool->enqueue(
                [this, packet, clientAddr]() { processUdpPacket(std::move(packet),clientAddr); });
            // TODO: enqueue task into udpThreadPool
          } else if (recvLen == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // TODO: handle recv error
          }
        }
      }
    } else if (numEvents == -1 && errno == EINTR) {
      // TODO: handle error and/or log it
      continue;
    }
  }

  // TODO: implement graceful ofload here or maybe in main run() method???


}


void Server::processUdpPacket(std::vector<char> packet, const sockaddr_in& clientAddr){
  {
    std::unique_lock<std::mutex> lock(sessionMutex);
  }
}
