#pragma once
#include "imsi.h"
#include <chrono>
class Session{
public:
    Session(IMSI imsi, std::chrono::time_point<std::chrono::steady_clock> expirationMoment);
    IMSI imsi;
    std::chrono::time_point<std::chrono::steady_clock> expiration;
};