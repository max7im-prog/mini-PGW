#include "server.h"
#include "CDREvent.h"
#include "nlohmann/json_fwd.hpp"
#include "spdlog/common.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/spdlog.h"
#include <arpa/inet.h>
#include <chrono>
#include <condition_variable>
#include <fcntl.h>
#include <fstream>
#include <httplib.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <thread>

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

  if (!data.contains("maxShutdownTimeSec") ||
      !data["maxShutdownTimeSec"].is_number_unsigned())
    return std::nullopt;
  serverConfig.maxShutdownTimeSec = data["maxShutdownTimeSec"].get<uint32_t>();

  if (!data.contains("preferredShutdownRateSessionsPerSec") ||
      !data["preferredShutdownRateSessionsPerSec"].is_number_unsigned())
    return std::nullopt;
  serverConfig.preferredShutdownRateSessionsPerSec =
      data["preferredShutdownRateSessionsPerSec"].get<uint32_t>();

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

  udpSocketContext.recvBuffer.resize(2048);

  loggingContext.serverLogger = spdlog::rotating_logger_mt(
      "serverLogger", config.logFileName, 1048576 * 5, 3);
  if (loggingContext.serverLogger == nullptr) {
    return false;
  }
  loggingContext.serverLogger->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
  if (config.logLevel == "INFO") {
    loggingContext.serverLogger->set_level(spdlog::level::info);
  } else if (config.logLevel == "CRIT") {
    loggingContext.serverLogger->set_level(spdlog::level::critical);
  } else if (config.logLevel == "DEBUG") {
    loggingContext.serverLogger->set_level(spdlog::level::debug);
  } else if (config.logLevel == "ERROR") {
    loggingContext.serverLogger->set_level(spdlog::level::err);
  } else if (config.logLevel == "WARN") {
    loggingContext.serverLogger->set_level(spdlog::level::warn);
  } else if (config.logLevel == "TRACE") {
    loggingContext.serverLogger->set_level(spdlog::level::trace);
  } else if (config.logLevel == "OFF") {
    loggingContext.serverLogger->set_level(spdlog::level::off);
  }

  loggingContext.cdrLogger = spdlog::rotating_logger_mt(
      "cdrLogger", config.cdrFileName, 1048576 * 5, 3);
  if (loggingContext.cdrLogger == nullptr) {
    return false;
  }
  loggingContext.cdrLogger->set_pattern("%v");
  loggingContext.cdrLogger->set_level(spdlog::level::info);

  spdlog::flush_every(std::chrono::seconds(1));

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

  logEvent("Epoll thread running");

  while (this->running) {
    int numEvents = epoll_wait(udpSocketContext.epollFD, events, MAX_EVENTS,
                               EPOLL_TIMEOUT_MSEC);

    if (numEvents > 0) {
      for (int i = 0; i < numEvents; i++) {
        if ((events[i].events & EPOLLIN) &&
            events[i].data.fd == udpSocketContext.udpSocketFD) {

          sockaddr_in clientAddr{};
          socklen_t addrLen = sizeof(clientAddr);
          ssize_t recvLen;

          do {
            recvLen =
                recvfrom(udpSocketContext.udpSocketFD,
                         udpSocketContext.recvBuffer.data(),
                         udpSocketContext.recvBuffer.size(), 0,
                         reinterpret_cast<sockaddr *>(&clientAddr), &addrLen);
          } while (recvLen == -1 && errno == EINTR);

          if (recvLen > 0) {
            std::vector<unsigned char> packet(
                udpSocketContext.recvBuffer.begin(),
                udpSocketContext.recvBuffer.begin() + recvLen);
            this->udpThreadPool->enqueue([this, packet, clientAddr]() {
              processUdpPacket(std::move(packet), clientAddr);
            });
          } else if (recvLen == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
            logEvent(std::string("Recvfrom error: ") + strerror(errno),
                     spdlog::level::err);
          }
        }
      }
    } else if (numEvents == -1) {
      if (errno == EINTR) {
        continue;
      }
      logEvent(std::string("epoll_wait error: ") + strerror(errno),
               spdlog::level::err);
    }
  }
}

void Server::processUdpPacket(std::vector<unsigned char> packet,
                              const sockaddr_in &clientAddr) {
  CDREvent::EventType response = CDREvent::EventType::rejected;
  auto imsi = IMSI::fromBCDBytes(packet);

  if (imsi.has_value()) {
    {
      std::unique_lock<std::mutex> lock(sessionMutex);
      if (config.blacklist.find(imsi.value()) != config.blacklist.end()) {
        response = CDREvent::EventType::rejected;
      } else {
        std::chrono::time_point<std::chrono::steady_clock> newExpiration =
            std::chrono::steady_clock::now() +
            std::chrono::seconds(config.sessionTimeoutSec);
        if (sessions.find(imsi.value()) != sessions.end()) {
          response = CDREvent::EventType::prolonged;
          sessions[imsi.value()].expiration = newExpiration;
        } else {
          response = CDREvent::EventType::created;
          addSession(imsi.value(), newExpiration);
        }
      }
    }
  } else {
    {
      logEvent("Discarded packet with invalid IMSI", spdlog::level::debug);
    }
    response = CDREvent::EventType::wrongIMSI;
  }

  switch (response) {
  case CDREvent::EventType::created:
    sendUdpPacket("created", clientAddr);
    break;
  case CDREvent::EventType::rejected:
    sendUdpPacket("rejected", clientAddr);
    break;
  case CDREvent::EventType::wrongIMSI:
    sendUdpPacket("rejected", clientAddr);
    imsi = IMSI::fromStdString("0");
    break;
  case CDREvent::EventType::prolonged:
    sendUdpPacket("created",
                  clientAddr); // Still return "created" to client as he does
                               // not need to know the insides of the code
    break;
  case CDREvent::EventType::deleted:
    break;
  }

  if (imsi.has_value()) {
    CDREvent event(imsi.value(), std::chrono::system_clock::now(), response);
    logCDR(event);
  }
}

void Server::run() {
  logEvent("Server started", spdlog::level::info);
  running = true;
  epollThread = std::thread(&Server::runEpollThread, this);
  httpThread = std::thread(&Server::runHttpThread, this);
  cleanupThread = std::thread(&Server::runCleanupThread, this);

  std::cout << "Running server" << std::endl;
  std::cout << "UDP: " << config.ip << ":" << config.udpPort << std::endl;
  std::cout << "HTTP: " << config.ip << ":" << config.httpPort << std::endl;

  if (epollThread.joinable()) {
    epollThread.join();
    logEvent("Epoll thread joined");
  }
  if (httpThread.joinable()) {
    httpThread.join();
    logEvent("Http thread joined");
  }
  if (cleanupThread.joinable()) {
    cleanupThread.join();
    logEvent("Cleanup thread joined");
  }
  deinit();
  logEvent("Server stopped", spdlog::level::info);
  std::cout << "Server stopped" << std::endl;
}

void Server::sendUdpPacket(const std::string &response,
                           const sockaddr_in &clientAddr) {
  auto result = sendto(
      udpSocketContext.udpSocketFD, response.data(), response.size(), 0,
      reinterpret_cast<const sockaddr *>(&clientAddr), sizeof(clientAddr));
  if (result == -1) {
    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), ipStr, sizeof(ipStr));
    uint16_t port = ntohs(clientAddr.sin_port);
    std::ostringstream oss;
    oss << "Failed to send UDP packet to " << ipStr << ":" << port << ": "
        << strerror(errno);
    logEvent(oss.str(), spdlog::level::err);
  }
}

void Server::runCleanupThread() {
  logEvent("Cleanup thread running");
  {
    std::unique_lock<std::mutex> lock(sessionMutex);

    while (running) {
      if (cleanupContext.cleanupQueue.empty()) {
        cleanupContext.cleanupCV.wait(lock);
        continue;
      }

      auto now = std::chrono::steady_clock::now();
      auto next = cleanupContext.cleanupQueue.top();

      if (next.expiration > now) {
        cleanupContext.cleanupCV.wait_until(lock, next.expiration);
        continue;
      }

      // Process all expired sessions
      while (!cleanupContext.cleanupQueue.empty()) {
        auto topEntry = cleanupContext.cleanupQueue.top();
        if (topEntry.expiration > now)
          break;

        auto it = sessions.find(topEntry.imsi);
        if (it != sessions.end()) {
          if (it->second.expiration <= now) {
            // Session has expired
            CDREvent event(it->second.imsi, std::chrono::system_clock::now(),
                           CDREvent::EventType::deleted);
            sessions.erase(it);
            logCDR(event);
          } else {
            // Session has been prolonged, reschedule cleanup
            CleanupContext::ExpirationEntry newEntry{};
            newEntry.imsi = topEntry.imsi;
            newEntry.expiration = it->second.expiration;
            cleanupContext.cleanupQueue.push(newEntry);
          }
        }
        cleanupContext.cleanupQueue.pop();
      }
    }
  }

  // Graceful shutdown
  logEvent("Graceful shutdown started");
  std::vector<IMSI> toCleanup;
  {
    std::unique_lock<std::mutex> lock(sessionMutex);
    for (auto &pair : sessions) {
      toCleanup.push_back(pair.first);
    }
  }

  if (toCleanup.empty()) {
    logEvent("Graceful shutdown complete");
    return;
  }

  std::chrono::nanoseconds shutdownPeriod;
  if (config.maxShutdownTimeSec > 0 &&
      float(config.maxShutdownTimeSec) >
          float(toCleanup.size()) /
              float(config.preferredShutdownRateSessionsPerSec)) {
    shutdownPeriod = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>(
            1.0 / config.preferredShutdownRateSessionsPerSec));
  } else {
    shutdownPeriod = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>(config.maxShutdownTimeSec) /
        toCleanup.size());
  }

  {
    std::unique_lock<std::mutex> lock(sessionMutex);
    for (auto &imsi : toCleanup) {
      lock.unlock();
      std::this_thread::sleep_for(shutdownPeriod);
      lock.lock();

      CDREvent event(imsi, std::chrono::system_clock::now(),
                     CDREvent::EventType::deleted);
      sessions.erase(imsi);
      logCDR(event);
    }
  }
  logEvent("Graceful shutdown complete");
}

void Server::addSession(
    IMSI imsi, std::chrono::time_point<std::chrono::steady_clock> expiration) {
  sessions[imsi] = Session(imsi, expiration);

  CleanupContext::ExpirationEntry newEntry{};
  newEntry.expiration = expiration;
  newEntry.imsi = imsi;

  bool shouldNotify = cleanupContext.cleanupQueue.empty() ||
                      expiration < cleanupContext.cleanupQueue.top().expiration;

  cleanupContext.cleanupQueue.push(std::move(newEntry));

  if (shouldNotify) {
    cleanupContext.cleanupCV.notify_one();
    logEvent("Cleanup queue empty - notifying cleanup thread",
             spdlog::level::debug);
  }
}

void Server::runHttpThread() {
  httplib::Server svr;

  svr.Get("/stop",
          [&, this](const httplib::Request &req, httplib::Response &res) {
            logEvent("Http thread recieved /stop command - stopping",
                     spdlog::level::info);
            running = false;
            cleanupContext.cleanupCV.notify_one(); // Wake cleanup thread
            res.set_content("Server stopping", "text/plain");
            svr.stop();
          });

  svr.Get("/check_subscriber", [this](const httplib::Request &req,
                                      httplib::Response &res) {
    if (!req.has_param("imsi")) {
      res.status = 400;
      res.set_content("Missing 'imsi' parameter", "text/plain");
      logEvent("Check_subscriber called without an imsi", spdlog::level::info);
      return;
    }

    std::string result = "not active";
    std::string param = req.get_param_value("imsi");
    auto imsi = IMSI::fromStdString(param);
    if (imsi.has_value()) {
      bool found = false;
      {
        std::unique_lock<std::mutex> lock(sessionMutex);
        found = (sessions.find(imsi.value()) != sessions.end());
      }
      if (found) {
        result = "active";
      }
    }

    res.set_content(result, "text/plain");
    {
      std::ostringstream oss;
      oss << "Check_subscriber with imsi \"" << imsi->toStdString()
          << "\", status: " << result;
      logEvent(oss.str(), spdlog::level::info);
    }
  });

  logEvent("Http thread running");
  if (!svr.listen(config.ip, config.httpPort)) {
    std::cerr << "Failed to start HTTP server" << std::endl;
    return;
  }
}

void Server::logCDR(const CDREvent &cdrEvent) {
  loggingContext.cdrLogger->info(cdrEvent.toString());
}

void Server::logEvent(const std::string &msg, spdlog::level::level_enum level) {
  loggingContext.serverLogger->log(level, msg);
}