#include "server.h"
#include <iostream>

int main(int argc, char *argv[]){

    if(argc <2){
        std::cout <<"Usage: " <<argv[0] << " <config file>" << std::endl;
        return -1;
    }
    std::string configFile = argv[1];
    auto server = Server::fromConfigFile(configFile);

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