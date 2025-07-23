#pragma once
#include <string>
#include <optional>
#include <vector>

class IMSI{
public:
    static std::optional<IMSI> fromStdString(const std::string &imsiStr);
    static std::optional<IMSI> fromBCDBytes(const std::vector<unsigned char> &bcdStr);
    const std::string& toStdString() const;
    std::string toBCDBytes() const;
    IMSI(const IMSI &other) = default;
    IMSI(IMSI &&other) = default;
    IMSI& operator=(const IMSI& other) = default;
    IMSI& operator=(IMSI&& other) = default;

    // Lexicographical, for use with std::set, should not be used directly
    bool operator<(const IMSI &other) const;
private:
    IMSI()=default;
    IMSI(const std::string& imsiStr);
    std::string imsi;
};