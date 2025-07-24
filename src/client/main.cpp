//TODO: rewrite because it's written by chatgpt


#include <iostream>
#include <string>
#include <vector>
#include <cstring>      // for memset
#include <arpa/inet.h>  // for sockaddr_in, inet_pton
#include <sys/socket.h> // for socket functions
#include <unistd.h>     // for close()
#include "imsi.h"

// Helper: convert IMSI string to vector of bytes (simple ASCII bytes here)
std::vector<unsigned char> imsiToBytes(const std::string& imsi) {
    return std::vector<unsigned char>(imsi.begin(), imsi.end());
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <IMSI>\n";
        return 1;
    }

    const std::string imsi = argv[1];
    auto temp = IMSI::fromStdString(imsi);
    if(!temp.has_value()){
        std::cerr <<"wrong imsi format" << std::endl;
        return -1;
    }
    auto imsiBytes = temp.value().toBCDBytes();

    const char* serverIp = "127.0.0.1";
    const int serverPort = 8080;

    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverIp, &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address: " << serverIp << "\n";
        close(sock);
        return 1;
    }

    // Send IMSI bytes
    ssize_t sent = sendto(sock, imsiBytes.data(), imsiBytes.size(), 0,
                          (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (sent < 0) {
        perror("sendto");
        close(sock);
        return 1;
    }

    // Receive response
    char buffer[1024] = {};
    sockaddr_in fromAddr{};
    socklen_t fromLen = sizeof(fromAddr);
    ssize_t recvLen = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                               (sockaddr*)&fromAddr, &fromLen);
    if (recvLen < 0) {
        perror("recvfrom");
        close(sock);
        return 1;
    }
    buffer[recvLen] = '\0';  // Null-terminate received string

    std::cout << "Received response: " << buffer << "\n";

    close(sock);
    return 0;
}
