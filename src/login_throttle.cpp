/**
 * @file src/login_throttle.cpp
 * @brief Slows down online password guessing against the web console.
 */
#include "login_throttle.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>

namespace login_throttle {

  namespace {
    constexpr int kFreeFailures = 5;
    constexpr int kBaseLockSeconds = 30;
    constexpr int kMaxLockRounds = 5;  // 30 s * 2^5 = 16 minutes
    constexpr std::size_t kMaxTracked = 2048;
    constexpr std::int64_t kForgetAfterSeconds = 24 * 3600;

    struct entry_t {
      int failures = 0;
      int rounds = 0;
      std::int64_t locked_until = 0;
      std::int64_t last_seen = 0;
    };

    std::mutex g_mutex;
    std::map<std::string, entry_t> g_entries;
    std::int64_t g_fixed_clock = 0;

    std::int64_t now_seconds() {
      if (g_fixed_clock != 0) {
        return g_fixed_clock;
      }
      return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count() + 1;
    }

    void prune(std::int64_t now) {
      if (g_entries.size() < kMaxTracked) {
        return;
      }
      for (auto it = g_entries.begin(); it != g_entries.end();) {
        it = (it->second.locked_until <= now && it->second.last_seen + kForgetAfterSeconds <= now) ? g_entries.erase(it) : std::next(it);
      }
      // Still full of fresh entries: drop the quietest ones rather than grow without bound.
      while (g_entries.size() >= kMaxTracked) {
        auto oldest = std::min_element(g_entries.begin(), g_entries.end(), [](const auto &a, const auto &b) {
          return a.second.last_seen < b.second.last_seen;
        });
        g_entries.erase(oldest);
      }
    }
  }  // namespace

  decision_t check(const std::string &address) {
    std::lock_guard lock {g_mutex};
    const auto now = now_seconds();
    const auto it = g_entries.find(address);
    if (it != g_entries.end() && it->second.locked_until > now) {
      return {false, static_cast<int>(it->second.locked_until - now)};
    }
    return {true, 0};
  }

  void record(const std::string &address, bool success) {
    std::lock_guard lock {g_mutex};
    const auto now = now_seconds();
    if (success) {
      g_entries.erase(address);
      return;
    }
    prune(now);
    auto &entry = g_entries[address];
    entry.last_seen = now;
    if (++entry.failures >= kFreeFailures) {
      entry.failures = 0;
      const int rounds = std::min(entry.rounds, kMaxLockRounds);
      entry.locked_until = now + (static_cast<std::int64_t>(kBaseLockSeconds) << rounds);
      ++entry.rounds;
    }
  }

  void set_clock_for_tests(std::int64_t unix_seconds) {
    std::lock_guard lock {g_mutex};
    g_fixed_clock = unix_seconds;
  }

  void reset_for_tests() {
    std::lock_guard lock {g_mutex};
    g_entries.clear();
  }

}  // namespace login_throttle
