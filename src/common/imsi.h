#pragma once
#include <optional>
#include <string>
#include <vector>

/**
 * @class IMSI
 * @brief Class to represent an IMSI
 *
 */
class IMSI {
public:
  /**
   * @brief Factory method to create an imsi from std::string, returns
   * std::nullopt on failure
   *
   * @return std::optional<IMSI>
   */
  static std::optional<IMSI> fromStdString(const std::string &imsiStr);

  /**
   * @brief Factory method to create an imsi from BCD (Binary Coded Decimal)
   * representation, returns std::nullopt on failure
   *
   * @return std::optional<IMSI>
   */
  static std::optional<IMSI>
  fromBCDBytes(const std::vector<unsigned char> &bcdStr);

  /**
   * @brief Method to get a string representation of an IMSI
   *
   * @return const std::string&
   */
  const std::string &toStdString() const;

  /**
   * @brief Method to get a BCD (Binary Coded Decimal) representation of IMSI
   *
   * @return std::vector<unsigned char>
   */
  std::vector<unsigned char> toBCDBytes() const;
  IMSI(const IMSI &other) = default;
  IMSI(IMSI &&other) = default;
  IMSI &operator=(const IMSI &other) = default;
  IMSI &operator=(IMSI &&other) = default;
  IMSI() = default;

  // Lexicographical, for use with std::set, should not be used directly
  bool operator<(const IMSI &other) const;

  bool operator==(const IMSI &other) const;

protected:
  IMSI(const std::string &imsiStr);
  std::string imsi;
};