#include "imsi.h"
#include "threadPool.h"
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
    threadPool->enqueue([] {}); // Should still run fine
  });
}


