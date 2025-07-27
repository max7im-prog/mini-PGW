#pragma once
#include "imsi.h"
#include <chrono>

/**
 * @class Session
 * @brief Class to represent a session
 *
 */
class Session {
public:
  Session(IMSI imsi,
          std::chrono::time_point<std::chrono::steady_clock> expirationMoment);
  Session() = default;
  IMSI imsi;
  std::chrono::time_point<std::chrono::steady_clock> expiration;

  Session(Session &other) = default;
  Session(Session &&other) = default;
  Session &operator=(Session &other) = default;
  Session &operator=(Session &&other) = default;
};