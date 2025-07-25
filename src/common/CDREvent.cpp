#include "CDREvent.h"
#include "imsi.h"
#include <chrono>
#include <sstream>

CDREvent::CDREvent(const IMSI &imsi, Timestamp timestamp, EventType eventType)
    : imsi(imsi), timestamp(timestamp), eventType(eventType) {}

#include <chrono>
#include <iomanip>
#include <sstream>

std::string CDREvent::toString() const {
  std::ostringstream oss;
  std::time_t temp = std::chrono::system_clock::to_time_t(timestamp);
  std::tm tm_buf;
  localtime_r(&temp, &tm_buf);
  oss << std::put_time(&tm_buf, "%Y/%m/%d/%H/%M");

  if (eventType != EventType::wrongIMSI) {
    oss << " " << imsi.toStdString();
    oss << " ";
  }
  switch (eventType) {
  case EventType::created:
    oss << "created";
    break;
  case EventType::rejected:
    oss << "rejected";
    break;
  case EventType::deleted:
    oss << "deleted";
    break;
  case EventType::prolonged:
    oss << "prolonged";
    break;
  case EventType::wrongIMSI:
    oss << "Invalid IMSI";
    break;
  }
  return oss.str();
}
