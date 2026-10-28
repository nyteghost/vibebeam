/**
 * @file src/two_factor.h
 * @brief Optional two-factor (TOTP) check for starting or resuming a stream.
 *
 * Vibebeam addition. The shared secret and the hashed backup codes live in
 * "two_factor.json" next to the host configuration (owner-only permissions).
 * Deleting that file turns the feature off, which is also the documented
 * recovery path for a locked-out owner.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace two_factor {

  /// Query parameter the client sends its code in.
  inline constexpr std::string_view kCodeParameter = "vibebeam_2fa";
  /// How long a verified client may reconnect without being asked again.
  inline constexpr int kGraceSeconds = 120;

  enum class result_e {
    ok,  ///< Allowed to continue.
    required,  ///< A code is needed and none was sent.
    invalid,  ///< The code was wrong, expired or already used.
    locked,  ///< Too many wrong codes recently; try again later.
    unavailable,  ///< 2FA is on but its settings cannot be read; refuse rather than fail open.
  };

  // ---- Building blocks (public so they can be tested) ----
  std::string base32_encode(const std::vector<std::uint8_t> &data);
  bool base32_decode(std::string_view text, std::vector<std::uint8_t> &out);
  /// RFC 4226/6238 code (HMAC-SHA1) for a time step.
  std::string totp_code(const std::vector<std::uint8_t> &secret, std::uint64_t step, int digits = 6);

  // ---- Host-side use ----
  bool enabled();
  /// Decide whether a launch/resume may continue. `exempt` is the per-client skip permission.
  result_e check(const std::string &client_uuid, std::string_view code, bool exempt);
  const char *status_message(result_e result);
  int status_code(result_e result);

  // ---- Web console management ----
  struct status_t {
    bool enabled;
    bool setup_pending;
    std::size_t backup_codes_remaining;
  };
  status_t status();
  /// Start enrolment; returns {secret, uri}. Nothing is enforced until confirm_setup succeeds.
  nlohmann::json begin_setup(const std::string &account_label);
  /// Confirm with one code from the authenticator app; returns the one-time backup codes.
  std::optional<std::vector<std::string>> confirm_setup(std::string_view code);
  /// Turn 2FA off after checking a current code (or a backup code).
  result_e disable(std::string_view code);

  /// Where the state file lives (call once at startup; until then 2FA is off), plus test-only clock/reset hooks.
  void set_state_path(const std::filesystem::path &path);

  /// Local recovery: delete the 2FA state (secret, backup codes) so 2FA is off again. Returns true if 2FA is now off.
  /// Only used by the `--reset-2fa` command line option, which needs shell access to the host.
  bool reset_state();
  void set_clock_for_tests(std::int64_t unix_seconds);
  void reset_runtime_for_tests();

}  // namespace two_factor
