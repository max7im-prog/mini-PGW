#include "CDREvent.h"
#include "imsi.h"
#include <chrono>
#include <sstream>

CDREvent::CDREvent(const IMSI &imsi, Timestamp timestamp, EventType eventType)
    : imsi(imsi), timestamp(timestamp), eventType(eventType) {}

#include <chrono>
#include <iomanip>
#include <sstream>

std::string CDREvent::toString() {
  std::ostringstream oss;
  std::time_t temp = std::chrono::system_clock::to_time_t(timestamp);
  oss << std::put_time(std::localtime(&temp), "%Y%m%d%H%M");
  oss << " ";
  oss << imsi.toStdString();
  oss << " ";
  switch (eventType) {
  case EventType::created:
    oss << "created session";
    break;
  case EventType::rejected:
    oss << "rejected session";
    break;
  case EventType::deleted:
    oss << "deleted session";
    break;
  }
  return oss.str();
}
