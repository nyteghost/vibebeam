#include "../tests_common.h"
#include "src/physical_display_baseline.h"
#include "src/repeated_failure_log.h"

using nlohmann::json;

TEST(PhysicalBaseline, DisabledMonitorIsNotActiveAndNullNamesUseIdentity) {
  const auto result = remote_display_topology::parse_physical_baseline(json::array({
    {{"device_id", "physical"}, {"friendly_name", nullptr}, {"info", nullptr}},
    {{"device_id", "active"}, {"display_name", "DP-1"}, {"info", {{"primary", true}}}}
  }));
  ASSERT_TRUE(result);
  ASSERT_EQ(result->size(), 2);
  EXPECT_FALSE((*result)[0].active);
  EXPECT_FALSE((*result)[0].primary);
  EXPECT_EQ((*result)[0].label, "physical");
  EXPECT_TRUE((*result)[1].active);
  EXPECT_TRUE((*result)[1].primary);
}

TEST(PhysicalBaseline, MalformedObservationCannotProducePartialReplacement) {
  const json good = {{"device_id", "one"}, {"info", {{"primary", false}}}};
  for (const auto &bad : {json(nullptr), json(7), json::object(), json {{"device_id", nullptr}},
                          json {{"device_id", "two"}, {"info", false}},
                          json {{"device_id", "two"}, {"info", {{"primary", nullptr}}}}}) {
    EXPECT_FALSE(remote_display_topology::parse_physical_baseline(json::array({good, bad})));
  }
  EXPECT_FALSE(remote_display_topology::parse_physical_baseline(json::array()));
  EXPECT_FALSE(remote_display_topology::parse_physical_baseline(json::array({good, good})));
  EXPECT_FALSE(remote_display_topology::parse_physical_baseline(nullptr));
}

TEST(RepeatedFailure, FirstChangedPeriodicAndRecoveryRemainVisible) {
  logging_policy::repeated_failure_t log;
  const auto start = std::chrono::steady_clock::time_point {};
  EXPECT_EQ(log.failure(0, start), 0);
  EXPECT_FALSE(log.failure(0, start + std::chrono::minutes {1}));
  EXPECT_EQ(log.failure(1, start + std::chrono::minutes {2}), 1);
  EXPECT_FALSE(log.failure(1, start + std::chrono::minutes {3}));
  EXPECT_EQ(log.failure(1, start + std::chrono::minutes {17}), 1);
  EXPECT_FALSE(log.failure(1, start + std::chrono::minutes {18}));
  EXPECT_EQ(log.recover(), 1);
  EXPECT_FALSE(log.recover());
  EXPECT_EQ(log.failure(1, start + std::chrono::minutes {19}), 0);
}
