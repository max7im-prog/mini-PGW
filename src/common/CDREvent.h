#pragma once
#include "imsi.h"
#include <chrono>

using Timestamp = std::chrono::time_point<std::chrono::system_clock>;

/**
 * @brief Class to represent a CDR (Call Detail Record) event.
 *
 */
class CDREvent {
public:
  enum class EventType {
    created,
    rejected,
    deleted,
    prolonged,
    wrongIMSI,
  };

  CDREvent(const IMSI &imsi, Timestamp timestamp, EventType eventType);
  std::string toString() const;

  IMSI imsi;
  Timestamp timestamp;
  EventType eventType;
};