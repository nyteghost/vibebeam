#include "../tests_common.h"
#include "src/physical_display_baseline.h"
#include "src/repeated_failure_log.h"
#include "src/platform/linux/dmabuf_modifier_query.h"

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

TEST(DmabufModifierQuery, DistinguishesExternalOnlyFromTwoDimensionalAndAbsent) {
  const auto query = [](int capacity, std::uint64_t *modifiers, unsigned int *external, int *count) {
    *count = 2;
    if (capacity) {
      modifiers[0] = 0;
      modifiers[1] = 7;
      external[0] = 1;
      external[1] = 0;
    }
    return true;
  };
  EXPECT_EQ((egl::query_modifier_support<std::uint64_t, unsigned int>(0, query)), egl::modifier_support_e::external_only);
  EXPECT_EQ((egl::query_modifier_support<std::uint64_t, unsigned int>(7, query)), egl::modifier_support_e::texture_2d);
  EXPECT_EQ((egl::query_modifier_support<std::uint64_t, unsigned int>(9, query)), egl::modifier_support_e::absent);
}

TEST(DmabufModifierQuery, RejectsUnboundedAndChangingDriverCounts) {
  for (const int initial : {-1, 0, 4097}) {
    int calls = 0;
    const auto query = [&](int, std::uint64_t *, unsigned int *, int *count) {
      ++calls;
      *count = initial;
      return true;
    };
    EXPECT_EQ((egl::query_modifier_support<std::uint64_t, unsigned int>(0, query)), egl::modifier_support_e::unknown);
    EXPECT_EQ(calls, 1);
  }
  for (const int returned : {-1, 2}) {
    const auto query = [&](int capacity, std::uint64_t *, unsigned int *, int *count) {
      *count = capacity ? returned : 1;
      return true;
    };
    EXPECT_EQ((egl::query_modifier_support<std::uint64_t, unsigned int>(0, query)), egl::modifier_support_e::unknown);
  }
  const auto failed = [](int, std::uint64_t *, unsigned int *, int *) { return false; };
  EXPECT_EQ((egl::query_modifier_support<std::uint64_t, unsigned int>(0, failed)), egl::modifier_support_e::unknown);
}
