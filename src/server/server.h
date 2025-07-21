#pragma once
#include <string>

class Server{
public:
    bool init(std::string configFile);
    void run();
};