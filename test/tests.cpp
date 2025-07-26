#include "imsi.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

TEST(BasicTest, TrueEqTrue) { EXPECT_EQ(true, true); }

TEST(TestIMSI, testFromStdString) {
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

TEST(TestIMSI, testBasicEqual) {
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