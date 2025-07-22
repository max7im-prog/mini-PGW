#pragma once
#include "imsi.h"
#include <chrono>

using Timestamp = std::chrono::time_point<std::chrono::system_clock>;

class CDREvent {
public:
  enum class EventType {
    created,
    rejected,
    deleted,
  };

  CDREvent(const IMSI &imsi, Timestamp timestamp, EventType eventType);
  std::string toString();

  IMSI imsi;
  Timestamp timestamp;
  EventType eventType;
};