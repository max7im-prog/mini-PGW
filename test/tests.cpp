#include "client.h"
#include "imsi.h"
#include "server.h"
#include "threadPool.h"
#include <condition_variable>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <string>

TEST(BasicTest, TrueEqTrue) { EXPECT_EQ(true, true); }

TEST(TestIMSI, fromStdString) {
  {
    auto imsi = IMSI::fromStdString("const std::string &imsiStr");
    EXPECT_FALSE(imsi.has_value());
  }
  {
    auto imsi = IMSI::fromStdString("123456789123456789"); // > 15 digits
    EXPECT_FALSE(imsi.has_value());
  }
  {
    auto imsi = IMSI::fromStdString("151515");
    EXPECT_TRUE(imsi.has_value());
  }
  {
    auto imsi = IMSI::fromStdString("123456789123456"); // = 15 digits
    EXPECT_TRUE(imsi.has_value());
  }
}

TEST(TestIMSI, basicEqual) {
  {
    auto imsi1 = IMSI::fromStdString("1598").value();
    auto imsi2 = IMSI::fromStdString("1598").value();
    EXPECT_EQ(imsi1, imsi2);
  }
  {
    auto imsi1 = IMSI::fromStdString("1598").value();
    auto imsi2 = IMSI::fromStdString("1599").value();
    EXPECT_NE(imsi1, imsi2);
  }
}

TEST(TestIMSI, fromToBCD) {
  {
    auto imsi1 = IMSI::fromStdString("1598").value();
    auto imsi2 = IMSI::fromStdString("1598").value();
    auto temp = imsi1.toBCDBytes();
    auto imsi3 = IMSI::fromBCDBytes(temp);
    EXPECT_EQ(imsi2, imsi3);
  }
  {
    auto imsi1 = IMSI::fromStdString("123456789123456").value();
    auto imsi2 = IMSI::fromStdString("123456789123456").value();
    auto temp = imsi1.toBCDBytes();
    auto imsi3 = IMSI::fromBCDBytes(temp);
    EXPECT_EQ(imsi2, imsi3);
  }
  {
    auto imsi1 = IMSI::fromStdString("1").value();
    auto imsi2 = IMSI::fromStdString("1").value();
    auto temp = imsi1.toBCDBytes();
    auto imsi3 = IMSI::fromBCDBytes(temp);
    EXPECT_EQ(imsi2, imsi3);
  }
}

TEST(TestIMSI, fromToStr) {
  {
    std::string imsiStr = "123456789";
    auto imsi = IMSI::fromStdString(imsiStr).value();
    EXPECT_EQ(imsi.toStdString(), imsiStr);
  }
  {
    std::string imsiStr = "123456789123456";
    auto imsi = IMSI::fromStdString(imsiStr).value();
    EXPECT_EQ(imsi.toStdString(), imsiStr);
  }
  {
    std::string imsiStr = "1";
    auto imsi = IMSI::fromStdString(imsiStr).value();
    EXPECT_EQ(imsi.toStdString(), imsiStr);
  }
}

TEST(TestIMSI, fromEmptyString) {
  auto imsi = IMSI::fromStdString("");
  EXPECT_FALSE(imsi.has_value());
}

TEST(TestIMSI, fromStringWithLetters) {
  auto imsi = IMSI::fromStdString("1234abc");
  EXPECT_FALSE(imsi.has_value());
}

TEST(TestIMSI, fromStringWithSpaces) {
  auto imsi = IMSI::fromStdString("12 34");
  EXPECT_FALSE(imsi.has_value());
}

TEST(TestIMSI, fromInvalidBCD) {
  std::vector<unsigned char> invalidBCD = {0xFF, 0xFF}; // not valid digits
  auto imsi = IMSI::fromBCDBytes(invalidBCD);
  EXPECT_FALSE(imsi.has_value());
}

class ThreadPoolTest : public ::testing::Test {
public:
  static void setupTestSuite() {}

  void SetUp() override {}

  void TearDown() override { threadPool.reset(); }

  static void TearDownTestSuite() {}

  std::unique_ptr<ThreadPool> threadPool;
};

TEST_F(ThreadPoolTest, createThreadPool) {
  threadPool = ThreadPool::create(5);
  EXPECT_NE(threadPool, nullptr);
}

TEST_F(ThreadPoolTest, failCreateThreadPool) {
  threadPool = ThreadPool::create(0);
  EXPECT_EQ(threadPool, nullptr);
}

TEST_F(ThreadPoolTest, enqueueTask) {
  threadPool = ThreadPool::create(5);
  ASSERT_NE(threadPool, nullptr);

  int numRepeats = 0;
  std::mutex mtx;
  std::condition_variable cv;
  int tasksRemaining = 10;

  for (int i = 0; i < 10; i++) {
    threadPool->enqueue([&]() {
      {
        std::lock_guard<std::mutex> lock(mtx);
        numRepeats++;
        tasksRemaining--;
      }
      cv.notify_one();
    });
  }

  bool allTasksFinished = false;
  {
    std::unique_lock<std::mutex> lock(mtx);
    allTasksFinished = cv.wait_for(lock, std::chrono::seconds(2),
                                   [&]() { return tasksRemaining == 0; });
    EXPECT_EQ(numRepeats, 10);
    EXPECT_EQ(tasksRemaining, 0);
  }

  EXPECT_TRUE(allTasksFinished);
}

TEST_F(ThreadPoolTest, ExceptionHandling) {
  threadPool = ThreadPool::create(2);
  EXPECT_NO_THROW({
    threadPool->enqueue([] { throw std::runtime_error("Oops"); });
    threadPool->enqueue([] {});
  });
}

class ServerMock : public Server {
public:
  void sendUdpPacket(const std::string &response,
                     const sockaddr_in &clientAddr) override {
    {
      std::lock_guard<std::mutex> lock(mockMutex);
      mockNumSent++;
    }
    mockCV.notify_one();
  }

  // void processUdpPacket(std::vector<unsigned char> packet,
  //                       const sockaddr_in &clientAddr) override {
  //   Server::processUdpPacket(std::move(packet), clientAddr);
  // }

  void mockProcessUdpPacket(std::vector<unsigned char> packet,
                            const sockaddr_in &clientAddr) {
    // Forward call to real Server implementation
    Server::processUdpPacket(std::move(packet), clientAddr);
  }

  void addSession(
      IMSI imsi,
      std::chrono::time_point<std::chrono::steady_clock> expiration) override {
    Server::addSession(imsi, expiration);
    {
      std::lock_guard<std::mutex> lock(mockMutex);
      mockNumSessions++;
    }
    mockCV.notify_one();
  }

  void stop() { running = false; }

  // Helper to wait until mockNumSent reaches a value
  bool waitForPackets(size_t expected, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mockMutex);
    return mockCV.wait_for(lock, timeout,
                           [&] { return mockNumSent >= expected; });
  }

  bool waitForSessions(size_t expected, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mockMutex);
    return mockCV.wait_for(lock, timeout,
                           [&] { return mockNumSessions >= expected; });
  }

  size_t getNumSent() {
    std::lock_guard<std::mutex> lock(mockMutex);
    return mockNumSent;
  }

  size_t getNumSessions() {
    std::lock_guard<std::mutex> lock(mockMutex);
    return mockNumSessions;
  }

  static std::unique_ptr<ServerMock>
  fromConfigMock(Server::ServerConfig config) {
    auto mock = std::unique_ptr<ServerMock>(new ServerMock());
    if (!mock->init(config))
      return nullptr;
    return mock;
  }

private:
  size_t mockNumSent = 0;
  size_t mockNumSessions = 0;
  std::condition_variable mockCV;
  std::mutex mockMutex;

  friend class ServerTest;
};

class ServerTest : public ::testing::Test {
public:
  static void setupTestSuite() {}
  void SetUp() override {
    config = ServerMock::parseConfigFile(configFileName).value();
  }
  void TearDown() override { server.reset(); }
  static void TearDownTestSuite() {}
  std::unique_ptr<ServerMock> server;
  Server::ServerConfig config;
  const std::string configFileName = "res/testServerConfig.json";
  friend class ServerMock;
};

TEST_F(ServerTest, ReadServerConfig) {
  auto temp = ServerMock::parseConfigFile(configFileName);
  ASSERT_NE(temp, std::nullopt);
}

TEST_F(ServerTest, FailOnWrongConfig) {
  ASSERT_NO_THROW(config = ServerMock::parseConfigFile(configFileName).value());
  {
    auto temp = config;
    config.ip = "455.455.455.455";
    auto svr = Server::fromConfig(config);
    EXPECT_EQ(svr, nullptr);
  }
  {
    auto temp = config;
    config.logLevel = "blablabla";
    auto svr = Server::fromConfig(config);
    EXPECT_EQ(svr, nullptr);
  }
}

TEST_F(ServerTest, SuccessOnGoodConfig) {
  ASSERT_NO_THROW(config = ServerMock::parseConfigFile(configFileName).value());
  {
    auto svr = ServerMock::fromConfig(config);
    EXPECT_NE(svr, nullptr) << "fail on fromConfig";
  }
  {
    auto svr = ServerMock::fromConfigFile(configFileName);
    EXPECT_NE(svr, nullptr) << "fail on fromConfigFile";
  }
}

TEST_F(ServerTest, ProcessUdpPackets) {
  config.blacklist = {*IMSI::fromStdString("1111")};
  auto svr = ServerMock::fromConfigMock(config);
  ASSERT_NE(svr, nullptr);
  {
    auto bcd = IMSI::fromStdString("1234")->toBCDBytes();
    sockaddr_in clientAddr;
    svr->mockProcessUdpPacket(bcd, clientAddr);
  }
  {
    auto bcd = IMSI::fromStdString("1111")->toBCDBytes();
    sockaddr_in clientAddr;
    svr->mockProcessUdpPacket(bcd, clientAddr);
  }
  {
    auto bcd = IMSI::fromStdString("1111")->toBCDBytes();
    sockaddr_in clientAddr;
    svr->mockProcessUdpPacket(bcd, clientAddr);
  }
  svr->waitForPackets(3, std::chrono::milliseconds(100));
  svr->waitForSessions(1, std::chrono::milliseconds(100));
  EXPECT_EQ(svr->getNumSessions(), 1);
  EXPECT_EQ(svr->getNumSent(), 3);
}

class ClientTest : public ::testing::Test {
public:
  static void setupTestSuite() {}
  void SetUp() override {
    config = Client::parseConfigFile(configFileName).value();
  }
  void TearDown() override { client.reset(); }
  static void TearDownTestSuite() {}
  std::unique_ptr<Client> client;
  Client::ClientConfig config;
  const std::string configFileName = "res/testClientConfig.json";
};

TEST_F(ClientTest, ParseConfig) {
  ASSERT_TRUE(Client::parseConfigFile(configFileName).has_value());
}

TEST_F(ClientTest, FromConfig) {
  {
    auto temp = config;
    auto client = Client::fromConfig(config);
    ASSERT_NE(client, nullptr);
  }
}

TEST_F(ClientTest, FromConfigFile) {
  {
    auto temp = config;
    auto client = Client::fromConfigFile(configFileName);
    ASSERT_NE(client, nullptr);
  }
}
