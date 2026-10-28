/**
 * @file src/login_throttle.h
 * @brief Slows down online password guessing against the web console (Vibebeam addition).
 *
 * Each remote address gets five wrong passwords; after that it is refused for a while, and
 * every further round of failures doubles the wait (30 s up to about 16 minutes). A correct
 * password clears the address.
 */
#pragma once

#include <cstdint>
#include <string>

namespace login_throttle {

  struct decision_t {
    bool allowed;
    int retry_after_seconds;
  };

  /// May this address try a password right now?
  decision_t check(const std::string &address);
  /// Record the outcome of a password check for this address.
  void record(const std::string &address, bool success);

  void set_clock_for_tests(std::int64_t unix_seconds);  // 0 = real clock
  void reset_for_tests();

}  // namespace login_throttle
