#pragma once
// This test-only boundary substitutes native Windows handles while compiling
// unchanged production display functions. Real gate behavior has separate tests.
#include <atomic>
#include <functional>
namespace display_helper::borrow {
  inline std::atomic<bool> &helper_process() {
    static std::atomic<bool> value {false}; return value;
  }
  class native_mutation_scope_t {
  public:
    static native_mutation_scope_t enter(bool, const std::function<bool()> & = {}) { return {}; }
    explicit operator bool() const noexcept { return true; }
  };
}
