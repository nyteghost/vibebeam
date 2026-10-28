/**
 * @file tests/unit/test_login_throttle.cpp
 * @brief Test src/login_throttle.cpp
 */

#include "src/login_throttle.h"

#include <gtest/gtest.h>

namespace {
  class LoginThrottleTest: public ::testing::Test {
  protected:
    void SetUp() override {
      login_throttle::reset_for_tests();
      login_throttle::set_clock_for_tests(1'000'000);
    }

    void TearDown() override {
      login_throttle::set_clock_for_tests(0);
      login_throttle::reset_for_tests();
    }
  };
}  // namespace

TEST_F(LoginThrottleTest, FiveWrongPasswordsThenLocked) {
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(login_throttle::check("10.0.0.5").allowed);
    login_throttle::record("10.0.0.5", false);
  }
  EXPECT_TRUE(login_throttle::check("10.0.0.5").allowed);
  login_throttle::record("10.0.0.5", false);
  const auto locked = login_throttle::check("10.0.0.5");
  EXPECT_FALSE(locked.allowed);
  EXPECT_EQ(locked.retry_after_seconds, 30);
  // Other addresses are unaffected.
  EXPECT_TRUE(login_throttle::check("10.0.0.6").allowed);
  // The lock ends.
  login_throttle::set_clock_for_tests(1'000'031);
  EXPECT_TRUE(login_throttle::check("10.0.0.5").allowed);
}

TEST_F(LoginThrottleTest, WaitDoublesEachRound) {
  for (int i = 0; i < 5; ++i) login_throttle::record("a", false);
  EXPECT_EQ(login_throttle::check("a").retry_after_seconds, 30);
  login_throttle::set_clock_for_tests(1'000'100);
  for (int i = 0; i < 5; ++i) login_throttle::record("a", false);
  EXPECT_EQ(login_throttle::check("a").retry_after_seconds, 60);
}

TEST_F(LoginThrottleTest, CorrectPasswordClears) {
  for (int i = 0; i < 4; ++i) login_throttle::record("a", false);
  login_throttle::record("a", true);
  for (int i = 0; i < 4; ++i) login_throttle::record("a", false);
  EXPECT_TRUE(login_throttle::check("a").allowed);
}
