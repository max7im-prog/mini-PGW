#include "imsi.h"
#include <cstdint>

const std::string &IMSI::toStdString() const { return imsi; }

std::string IMSI::toBCDBytes() const {
  std::string bcd;
  uint16_t imsiSizeBytes = (imsi.size() + 1) / 2;
  bcd.reserve(4 + imsiSizeBytes);

  // Header as defined in TS 29.274 §8.3
  bcd.push_back(1);
  bcd.push_back(static_cast<unsigned char>(imsiSizeBytes >> 8));
  bcd.push_back(static_cast<unsigned char>(imsiSizeBytes & 0xFF));
  bcd.push_back(0);

  // IMSI itself
  for (size_t i = 0; i < imsi.size(); i += 2) {
    uint8_t low = imsi[i] - '0';
    uint8_t high = 0b1111;

    if (i + 1 < imsi.size()) {
      high = imsi[i + 1] - '0';
    }

    unsigned char next = (high << 4) | low;
    bcd.push_back(next);
  }

  return bcd;
}

std::optional<IMSI> IMSI::fromBCDBytes(const std::string &bcdStr) {
  static constexpr uint8_t headerSize = 4;
  std::string imsiStr;

  // Check correctness of a header
  if (bcdStr.size() < headerSize) {
    return std::nullopt;
  }
  uint16_t imsiSize = (static_cast<uint16_t>(bcdStr[1]) << 8) |
                      ((static_cast<uint16_t>(bcdStr[2])) & 0xFF);
  imsiStr.reserve(imsiSize * 2);
  if (imsiSize != bcdStr.size() - headerSize) {
    return std::nullopt;
  }

  // Read IMSI bytes from BCD encoding
  for (size_t i = headerSize; i < bcdStr.size(); i++) {
    uint8_t low = bcdStr[i] & 0xF;
    uint8_t high = bcdStr[i] >> 4;
    if (0 <= low && low <= 9) {
      imsiStr.push_back(static_cast<unsigned char>(low + '0'));
    } else {
      return std::nullopt;
    }
    if (0 <= high && high <= 9) {
      imsiStr.push_back(static_cast<unsigned char>(high + '0'));
    } else if (!(i == bcdStr.size() - 1 && high == 0b1111)) {
      return std::nullopt;
    }
  }

  IMSI imsi(imsiStr);
  return imsi;
}

std::optional<IMSI> IMSI::fromStdString(const std::string &imsiStr) {
  static constexpr size_t maxIMSISize = 15;
  if (imsiStr.size() == 0 || imsiStr.size() > maxIMSISize) {
    return std::nullopt;
  }
  for (auto ch : imsiStr) {
    if (ch < '0' || ch > '9') {
      return std::nullopt;
    }
  }

  IMSI imsi(imsiStr);
  return imsi;
}

IMSI::IMSI(const std::string &imsiStr) : imsi(imsiStr) {}

bool IMSI::operator<(const IMSI &other) const { return imsi < other.imsi; }
