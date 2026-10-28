/**
 * @file src/platform/linux/display_health.h
 * @brief Read-only checks that explain why the Linux virtual display may be unavailable.
 */
#pragma once

// lib includes
#include <nlohmann/json.hpp>

namespace platf::linux_display_health {
  /**
   * @brief Inspect the running kernel, the Vibebeam DRM module and the private connectors.
   *
   * Only reads sysfs and DKMS state; it never runs a program and never needs privileges.
   * @return JSON with `checks` (id, ok, detail), `summary`, `healthy` and `fix_commands`.
   */
  nlohmann::json collect();

  /**
   * @brief Render collect() as plain text for the diagnostics download.
   */
  std::string to_text(const nlohmann::json &health);
}  // namespace platf::linux_display_health
