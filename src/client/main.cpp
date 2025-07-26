#include "client.h"
#include "imsi.h"
#include <iostream>
#include <string>

// Helper: convert IMSI string to vector of bytes (simple ASCII bytes here)
std::vector<unsigned char> imsiToBytes(const std::string &imsi) {
  return std::vector<unsigned char>(imsi.begin(), imsi.end());
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <config file>" << " <IMSI>"
              << std::endl;
    return 1;
  }

  auto client = Client::fromConfigFile(argv[1]);
  if (client == nullptr) {
    std::cerr << "Failed to create client" << std::endl;
    return 1;
  }

  auto imsi = IMSI::fromStdString(argv[2]);
  if (!imsi.has_value()) {
    std::cerr << "Invalid imsi" << std::endl;
    return 1;
  }

  client->run(imsi.value());

  return 0;
}
