#include "server.h"
#include <iostream>

int main(int argc, char *argv[]){
    Server::ServerConfig config = {};
    config.blacklist = {};
    config.blacklist.insert(IMSI::fromStdString("111").value());
    config.ip = "127.0.0.1";
    config.udpPort = 8080;
    config.httpPort = 9000;
    config.numUdpThreads = 5;
    config.sessionTimeoutSec = 3;
    config.logLevel = "DEBUG";
    config.logFileName = "logs/server.log";
    config.cdrFileName = "logs/cdr.log";

    auto server = Server::fromConfig(config);
    if (!server) {
        std::cerr << "Failed to create server.\n";
        return EXIT_FAILURE;
    }

    try {
        server->run();
    } catch (const std::exception &e) {
        std::cerr << "Server crashed: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}