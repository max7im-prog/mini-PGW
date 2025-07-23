#pragma once
#include "imsi.h"
#include <chrono>
class Session{
public:
    IMSI imsi;
    std::chrono::time_point<std::chrono::steady_clock> expiration;
};