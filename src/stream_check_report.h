#pragma once
#include <nlohmann/json.hpp>
#include <cmath>

namespace stream_check {
  inline bool authorized(bool verified_certificate, bool view_permission, bool own_session) {
    return verified_certificate && view_permission && own_session;
  }

  inline nlohmann::json capture_report(bool active, bool cpu_upload, unsigned samples, double mean_us, double max_us, double age_seconds) {
    if (!active || !samples || !std::isfinite(age_seconds) || age_seconds < 0 || age_seconds > 10 ||
        !std::isfinite(mean_us) || mean_us < 0 || !std::isfinite(max_us) || max_us < 0) return {{"status", "unavailable"}};
    return {{"status", "observed"}, {"scope", "host-wide managed capture; not attributed to this client"},
      {"path", cpu_upload ? "linear_cpu_upload" : "egl_gpu_import"}, {"samples", samples},
      {"cpu_submission_mean_us", mean_us}, {"cpu_submission_max_us", max_us}, {"age_seconds", age_seconds}};
  }

  // Fixed allowlist: never include raw logs, certificates, addresses, account
  // names, commands, environment variables or another client's identity.
  inline nlohmann::json base_report() {
    return {{"schema", 1}, {"status", "available"}, {"paired_client_has_session", true},
      {"capture", {{"status", "unavailable"}}},
      {"audio", {{"status", "needs_confirmation"}}},
      {"display_restoration", {{"status", "needs_confirmation"}}},
      {"cursor", {{"status", "needs_confirmation"}}}};
  }
}
