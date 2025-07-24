#include "session.h"
#include "imsi.h"

Session::Session(
    IMSI imsi,
    std::chrono::time_point<std::chrono::steady_clock> expirationMoment)
    : imsi(imsi), expiration(expirationMoment) {}