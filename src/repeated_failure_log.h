#pragma once

#include <chrono>
#include <optional>
#include <utility>

namespace logging_policy {
  // Report the first failure, changed errors, and periodic repeat summaries.
  // The retry itself is never suppressed. Instances belong to one operation.
  class repeated_failure_t {
  public:
    std::optional<unsigned int> failure(int reason, std::chrono::steady_clock::time_point now) {
      if (!last || reason != last_reason || now < *last || now - *last >= std::chrono::minutes {15}) {
        last = now;
        last_reason = reason;
        return std::exchange(suppressed, 0);
      }
      ++suppressed;
      return std::nullopt;
    }

    std::optional<unsigned int> recover() {
      if (!last) return std::nullopt;
      last.reset();
      return std::exchange(suppressed, 0);
    }

  private:
    std::optional<std::chrono::steady_clock::time_point> last;
    int last_reason = 0;
    unsigned int suppressed = 0;
  };
}
