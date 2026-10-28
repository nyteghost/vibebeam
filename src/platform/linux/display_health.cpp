/**
 * @file src/platform/linux/display_health.cpp
 * @brief Read-only checks that explain why the Linux virtual display may be unavailable.
 */
// standard includes
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// platform includes
#include <sys/utsname.h>

// local includes
#include "display_health.h"

using namespace std::literals;
namespace fs = std::filesystem;

namespace platf::linux_display_health {
  namespace {
    std::string kernel_release() {
      utsname info {};
      return uname(&info) == 0 ? info.release : "unknown";
    }

    /** Names of loaded kernel modules whose name contains "vibeshine". */
    std::vector<std::string> loaded_modules() {
      std::vector<std::string> out;
      std::error_code ec;
      for (fs::directory_iterator it {"/sys/module", ec}, end; !ec && it != end; it.increment(ec)) {
        const auto name = it->path().filename().string();
        if (name.find("vibeshine") != std::string::npos) {
          out.push_back(name);
        }
      }
      std::sort(out.begin(), out.end());
      return out;
    }

    /** Private connectors published by the Vibebeam virtual display device. */
    std::vector<std::string> private_connectors() {
      std::vector<std::string> out;
      std::error_code ec;
      for (fs::directory_iterator it {"/sys/class/drm", ec}, end; !ec && it != end; it.increment(ec)) {
        const auto filename = it->path().filename().string();
        const auto dash = filename.find('-');
        if (!filename.starts_with("card") || dash == std::string::npos) {
          continue;
        }
        std::error_code canonical_ec;
        const auto resolved = fs::canonical(it->path(), canonical_ec);
        if (!canonical_ec && resolved.string().find("/devices/faux/vibeshine") != std::string::npos) {
          out.push_back(filename.substr(dash + 1));
        }
      }
      std::sort(out.begin(), out.end());
      return out;
    }

    /** DKMS entries mentioning vibeshine, as "name/version: kernels built". */
    std::vector<std::string> dkms_entries(const std::string &kernel, bool &built_for_running_kernel) {
      std::vector<std::string> out;
      built_for_running_kernel = false;
      std::error_code ec;
      for (fs::directory_iterator it {"/var/lib/dkms", ec}, end; !ec && it != end; it.increment(ec)) {
        const auto name = it->path().filename().string();
        if (name.find("vibeshine") == std::string::npos) {
          continue;
        }
        std::error_code inner;
        for (fs::directory_iterator version {it->path(), inner}, vend; !inner && version != vend; version.increment(inner)) {
          if (!version->is_directory(inner)) {
            continue;
          }
          std::string kernels;
          std::error_code kernel_ec;
          for (fs::directory_iterator built {version->path(), kernel_ec}, bend; !kernel_ec && built != bend; built.increment(kernel_ec)) {
            const auto built_name = built->path().filename().string();
            if (built_name == "source" || built_name == "build" || built_name == "log") {
              continue;
            }
            if (!built->is_directory(kernel_ec)) {
              continue;
            }
            kernels += (kernels.empty() ? "" : ", ") + built_name;
            if (built_name == kernel) {
              built_for_running_kernel = true;
            }
          }
          out.push_back(name + "/" + version->path().filename().string() + ": " + (kernels.empty() ? "not built for any kernel"s : kernels));
        }
      }
      return out;
    }

    std::string join(const std::vector<std::string> &items, const std::string &separator = ", "s) {
      std::string out;
      for (const auto &item : items) {
        out += (out.empty() ? "" : separator) + item;
      }
      return out;
    }

    nlohmann::json check(const std::string &id, bool ok, const std::string &detail) {
      return {{"id", id}, {"ok", ok}, {"detail", detail}};
    }
  }  // namespace

  nlohmann::json collect() {
    const auto kernel = kernel_release();
    const auto modules = loaded_modules();
    const auto connectors = private_connectors();
    bool dkms_built = false;
    const auto dkms = dkms_entries(kernel, dkms_built);

    // A kernel update replaces the running kernel's module directory; until you reboot, the
    // system cannot load modules for it, so the virtual display cannot appear.
    std::error_code modules_ec;
    const bool kernel_files_present = fs::exists("/usr/lib/modules/" + kernel, modules_ec);

    nlohmann::json checks = nlohmann::json::array();
    checks.push_back(check("kernel", true, "Running kernel " + kernel));
    checks.push_back(check(
      "reboot",
      kernel_files_present,
      kernel_files_present ? "The running kernel's files are present" :
                             "The running kernel's files are gone: the system was updated since it started"
    ));
    checks.push_back(check(
      "dkms",
      dkms_built,
      dkms.empty() ? "No Vibebeam DKMS module is registered on this system" :
                     (dkms_built ? "Built for the running kernel (" + join(dkms, "; ") + ")" :
                                   "Not built for the running kernel (" + join(dkms, "; ") + ")")
    ));
    checks.push_back(check(
      "module",
      !modules.empty(),
      modules.empty() ? "The Vibebeam virtual display kernel module is not loaded" : "Loaded: " + join(modules)
    ));
    checks.push_back(check(
      "connectors",
      !connectors.empty(),
      connectors.empty() ? "No virtual display connectors are published, so there is nothing to stream to" :
                           std::to_string(connectors.size()) + " available: " + join(connectors)
    ));

    nlohmann::json fixes = nlohmann::json::array();
    std::string summary;
    if (!connectors.empty()) {
      summary = "The virtual display is ready.";
    } else if (!kernel_files_present) {
      summary = "The system was updated while it was running, so the running kernel can no longer load modules. Reboot to finish the update.";
      fixes.push_back("sudo reboot");
    } else if (!dkms_built) {
      summary = "The virtual display module has not been built for the kernel you are running. A kernel update usually causes this.";
      fixes.push_back("sudo pacman -S --needed linux-cachyos-headers   # use the headers package that matches your kernel");
      fixes.push_back("sudo /usr/libexec/vibeshine/vibeshine-drm-install install");
      fixes.push_back("sudo reboot");
    } else if (modules.empty()) {
      summary = "The module is built but not loaded. Reloading it, or rebooting, normally fixes this.";
      fixes.push_back("sudo /usr/libexec/vibeshine/vibeshine-drm-install install");
      fixes.push_back("sudo systemctl restart vibepollo.service");
    } else {
      summary = "The module is loaded but published no connectors. Restart the host, and if that does not help, reboot.";
      fixes.push_back("sudo systemctl restart vibepollo.service");
      fixes.push_back("sudo reboot");
    }

    return {
      {"healthy", !connectors.empty()},
      {"summary", summary},
      {"checks", checks},
      {"fix_commands", fixes},
    };
  }

  std::string to_text(const nlohmann::json &health) {
    std::ostringstream out;
    out << "Vibebeam virtual display health\n";
    out << "Summary: " << health.value("summary", "") << "\n\n";
    for (const auto &item : health.value("checks", nlohmann::json::array())) {
      out << (item.value("ok", false) ? "[ok]   " : "[FAIL] ") << item.value("id", "") << ": " << item.value("detail", "") << "\n";
    }
    if (health.contains("fix_commands") && !health["fix_commands"].empty()) {
      out << "\nSuggested fix:\n";
      for (const auto &command : health["fix_commands"]) {
        out << "  " << command.get<std::string>() << "\n";
      }
    }
    return out.str();
  }
}  // namespace platf::linux_display_health
