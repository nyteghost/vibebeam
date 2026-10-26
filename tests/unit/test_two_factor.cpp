/**
 * @file tests/unit/test_two_factor.cpp
 * @brief Test src/two_factor.cpp (TOTP, replay, rate limiting, backup codes).
 */

#include "src/two_factor.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace {
  using two_factor::result_e;

  const std::vector<std::uint8_t> kRfcSecret {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};

  class TwoFactorTest: public ::testing::Test {
  protected:
    std::filesystem::path path;

    void SetUp() override {
      path = std::filesystem::temp_directory_path() / ("two_factor_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name() + ".json");
      std::filesystem::remove(path);
      two_factor::set_state_path(path);
      two_factor::set_clock_for_tests(1'700'000'010);
      two_factor::reset_runtime_for_tests();
    }

    void TearDown() override {
      std::filesystem::remove(path);
      two_factor::set_clock_for_tests(0);
      two_factor::set_state_path({});
    }

    // Enable with a known secret by walking the real enrolment flow.
    std::vector<std::string> enable(std::uint64_t &step_used) {
      const auto setup = two_factor::begin_setup("host");
      std::vector<std::uint8_t> secret;
      EXPECT_TRUE(two_factor::base32_decode(setup["secret"].get<std::string>(), secret));
      const std::uint64_t step = 1'700'000'010 / 30;
      step_used = step;
      auto codes = two_factor::confirm_setup(two_factor::totp_code(secret, step));
      EXPECT_TRUE(codes.has_value());
      secret_ = secret;
      return codes.value_or(std::vector<std::string> {});
    }

    std::string code_for(std::uint64_t step) const {
      return two_factor::totp_code(secret_, step);
    }

    std::vector<std::uint8_t> secret_;
  };
}  // namespace

TEST(TwoFactorPrimitives, Rfc6238Vector) {
  EXPECT_EQ(two_factor::totp_code(kRfcSecret, 59 / 30, 8), "94287082");
  EXPECT_EQ(two_factor::totp_code(kRfcSecret, 1111111109 / 30, 8), "07081804");
}

TEST(TwoFactorPrimitives, Base32RoundTrip) {
  std::vector<std::uint8_t> out;
  ASSERT_TRUE(two_factor::base32_decode(two_factor::base32_encode(kRfcSecret), out));
  EXPECT_EQ(out, kRfcSecret);
  EXPECT_EQ(two_factor::base32_encode(kRfcSecret), "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ");
  EXPECT_FALSE(two_factor::base32_decode("not*base32", out));
}

TEST_F(TwoFactorTest, OffByDefaultAndAnyoneMayConnect) {
  EXPECT_FALSE(two_factor::enabled());
  EXPECT_EQ(two_factor::check("a", "", false), result_e::ok);
}

TEST_F(TwoFactorTest, EnrolmentNeedsAWorkingCode) {
  two_factor::begin_setup("host");
  EXPECT_FALSE(two_factor::confirm_setup("000000").has_value());
  EXPECT_FALSE(two_factor::enabled());
}

TEST_F(TwoFactorTest, RequiresCodeAndEachCodeWorksOnce) {
  std::uint64_t used = 0;
  enable(used);
  ASSERT_TRUE(two_factor::enabled());

  EXPECT_EQ(two_factor::check("phone", "", false), result_e::required);
  // The code used during enrolment must not be accepted again.
  EXPECT_EQ(two_factor::check("phone", code_for(used), false), result_e::invalid);
  // The next time step is fine, then cannot be replayed from another client.
  two_factor::set_clock_for_tests(1'700'000'010 + 30);
  const auto next = code_for(used + 1);
  EXPECT_EQ(two_factor::check("phone", next, false), result_e::ok);
  EXPECT_EQ(two_factor::check("laptop", next, false), result_e::invalid);
}

TEST_F(TwoFactorTest, GraceWindowAvoidsReprompt) {
  std::uint64_t used = 0;
  enable(used);
  two_factor::set_clock_for_tests(1'700'000'040);
  ASSERT_EQ(two_factor::check("phone", code_for(used + 1), false), result_e::ok);
  two_factor::set_clock_for_tests(1'700'000'040 + 60);
  EXPECT_EQ(two_factor::check("phone", "", false), result_e::ok);
  EXPECT_EQ(two_factor::check("other", "", false), result_e::required);
  two_factor::set_clock_for_tests(1'700'000'040 + two_factor::kGraceSeconds + 1);
  EXPECT_EQ(two_factor::check("phone", "", false), result_e::required);
}

TEST_F(TwoFactorTest, ExemptClientsSkipTheCode) {
  std::uint64_t used = 0;
  enable(used);
  EXPECT_EQ(two_factor::check("ios", "", true), result_e::ok);
}

TEST_F(TwoFactorTest, WrongCodesAreRateLimited) {
  std::uint64_t used = 0;
  enable(used);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(two_factor::check("phone", "111111", false), result_e::invalid);
  }
  EXPECT_EQ(two_factor::check("phone", "111111", false), result_e::locked);
  // Even a correct code is refused while locked.
  EXPECT_EQ(two_factor::check("phone", code_for(used + 1), false), result_e::locked);
  // Another client is not locked out by this one.
  EXPECT_EQ(two_factor::check("other", "", false), result_e::required);
  two_factor::set_clock_for_tests(1'700'000'010 + 301);
  EXPECT_EQ(two_factor::check("phone", code_for(1'700'000'311 / 30), false), result_e::ok);
}

TEST_F(TwoFactorTest, BackupCodesWorkOnce) {
  std::uint64_t used = 0;
  const auto backups = enable(used);
  ASSERT_EQ(backups.size(), 8u);
  EXPECT_EQ(two_factor::status().backup_codes_remaining, 8u);
  EXPECT_EQ(two_factor::check("phone", backups[0], false), result_e::ok);
  EXPECT_EQ(two_factor::status().backup_codes_remaining, 7u);
  two_factor::reset_runtime_for_tests();  // forget the grace window
  EXPECT_EQ(two_factor::check("phone", backups[0], false), result_e::invalid);
}

TEST_F(TwoFactorTest, DisableNeedsACodeAndDeletingTheFileResets) {
  std::uint64_t used = 0;
  enable(used);
  EXPECT_EQ(two_factor::disable("123456"), result_e::invalid);
  EXPECT_TRUE(two_factor::enabled());
  std::filesystem::remove(path);  // the documented sudo reset
  EXPECT_FALSE(two_factor::enabled());
  EXPECT_EQ(two_factor::check("phone", "", false), result_e::ok);
}

TEST_F(TwoFactorTest, ResetStateTurnsTwoFactorOff) {
  std::uint64_t used = 0;
  enable(used);
  EXPECT_TRUE(two_factor::enabled());
  EXPECT_TRUE(two_factor::reset_state());
  EXPECT_FALSE(two_factor::enabled());
  EXPECT_EQ(two_factor::check("phone", "", false), result_e::ok);
  EXPECT_TRUE(two_factor::reset_state());  // already off is fine
}

TEST_F(TwoFactorTest, DisableWithBackupCode) {
  std::uint64_t used = 0;
  const auto backups = enable(used);
  EXPECT_EQ(two_factor::disable(backups[3]), result_e::ok);
  EXPECT_FALSE(two_factor::enabled());
}

TEST_F(TwoFactorTest, CorruptSettingsFailClosed) {
  std::uint64_t used = 0;
  enable(used);
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << "{not json";
  }
  EXPECT_TRUE(two_factor::enabled());
  EXPECT_EQ(two_factor::check("phone", "", false), result_e::unavailable);
  EXPECT_EQ(two_factor::check("phone", "123456", false), result_e::unavailable);
  // Exempt clients are unaffected, and the owner can still reset by deleting the file.
  EXPECT_EQ(two_factor::check("ios", "", true), result_e::ok);
  std::filesystem::remove(path);
  EXPECT_EQ(two_factor::check("phone", "", false), result_e::ok);
}
