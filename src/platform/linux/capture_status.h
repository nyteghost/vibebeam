/** Passive observations of live managed KMS capture, never encoder probes. */
#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>

namespace platf::linux_capture_status {
  struct transfer_observation_t {
    bool cpu_upload = false;
    unsigned int samples = 0;
    double mean_us = 0;
    double max_us = 0;
    std::chrono::steady_clock::time_point observed_at;
  };
  inline std::mutex transfer_mutex;
  inline std::optional<transfer_observation_t> last_managed_transfer;

  inline void observe_managed_transfer(transfer_observation_t observation) {
    // Diagnostics must never stall frame submission behind an HTTP reader.
    std::unique_lock lock {transfer_mutex, std::try_to_lock};
    if (lock.owns_lock()) last_managed_transfer = observation;
  }

  inline std::optional<transfer_observation_t> managed_transfer() {
    std::lock_guard lock {transfer_mutex};
    return last_managed_transfer;
  }

  inline std::atomic<unsigned> managed_event_captures {0};

  class managed_capture_scope {
  public:
    managed_capture_scope() noexcept {
      managed_event_captures.fetch_add(1, std::memory_order_relaxed);
    }
    ~managed_capture_scope() {
      managed_event_captures.fetch_sub(1, std::memory_order_relaxed);
    }
    managed_capture_scope(const managed_capture_scope &) = delete;
    managed_capture_scope &operator=(const managed_capture_scope &) = delete;
  };

  inline bool managed_event_capture_active() noexcept {
    return managed_event_captures.load(std::memory_order_relaxed) != 0;
  }
}
