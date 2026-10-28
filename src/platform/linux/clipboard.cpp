/**
 * @file src/platform/linux/clipboard.cpp
 * @brief Text clipboard access for Vibebeam on a Linux host.
 *
 * Backends, in the order they are tried:
 *  0. The root-run session broker (when Vibebeam runs as the hardened system service): the
 *     broker reaches Klipper inside the signed-in user's session on our behalf.
 *  1. KDE Klipper over the session D-Bus (Plasma, X11 or Wayland). No helper program is
 *     needed and it keeps working when no window has focus.
 *  2. wl-clipboard (wl-copy / wl-paste) on a Wayland session.
 *  3. xclip on an X11 session.
 * The choice is made once, logged once, and never includes clipboard contents in the log.
 */
// standard includes
#include <cerrno>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

// platform includes
#include <fcntl.h>
#include <gio/gio.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

// local includes
#include "src/logging.h"
#include "src/platform/common.h"

extern char **environ;

using namespace std::literals;

namespace platf {
  namespace {
    constexpr std::size_t max_clipboard_bytes = 4 * 1024 * 1024;
    constexpr int tool_timeout_ms = 2000;
    constexpr int dbus_timeout_ms = 2000;

    enum class backend_e {
      none,
      broker,
      klipper,
      wl_clipboard,
      xclip,
    };

    struct backend_state_t {
      backend_e backend = backend_e::none;
      std::string name;
      std::string detail;  // why nothing was found, or what was found
    };

    std::string env_or_empty(const char *name) {
      const char *value = std::getenv(name);
      return value ? value : "";
    }

    std::string runtime_dir() {
      auto dir = env_or_empty("XDG_RUNTIME_DIR");
      if (dir.empty()) {
        dir = "/run/user/" + std::to_string(getuid());
      }
      return dir;
    }

    /**
     * @brief Fill in the desktop-session variables a service started outside the session lacks.
     * @return Environment entries ("NAME=value") to add for child tools and D-Bus.
     */
    std::vector<std::string> session_env_fixups() {
      std::vector<std::string> extra;
      const auto dir = runtime_dir();
      if (env_or_empty("XDG_RUNTIME_DIR").empty()) {
        extra.push_back("XDG_RUNTIME_DIR=" + dir);
      }
      if (env_or_empty("DBUS_SESSION_BUS_ADDRESS").empty()) {
        extra.push_back("DBUS_SESSION_BUS_ADDRESS=unix:path=" + dir + "/bus");
      }
      if (env_or_empty("WAYLAND_DISPLAY").empty()) {
        std::error_code ec;
        for (int i = 0; i < 4; ++i) {
          const auto name = "wayland-"s + std::to_string(i);
          if (std::filesystem::exists(dir + "/" + name, ec)) {
            extra.push_back("WAYLAND_DISPLAY=" + name);
            break;
          }
        }
      }
      return extra;
    }

    bool in_path(const std::string &tool) {
      const auto path = env_or_empty("PATH");
      std::size_t start = 0;
      while (start <= path.size()) {
        auto end = path.find(':', start);
        if (end == std::string::npos) {
          end = path.size();
        }
        const auto dir = path.substr(start, end - start);
        if (!dir.empty() && access((dir + "/" + tool).c_str(), X_OK) == 0) {
          return true;
        }
        start = end + 1;
      }
      return false;
    }

    /**
     * @brief Run a helper program with optional stdin data and a timeout.
     * @param argv Program and arguments.
     * @param input Data written to the program's stdin.
     * @param want_output Capture stdout into `output` (stderr is always discarded).
     * @param output Receives stdout when `want_output` is true.
     * @return true when the program exited with status 0 before the timeout.
     */
    bool run_tool(const std::vector<std::string> &argv, const std::string &input, bool want_output, std::string &output) {
      int in_pipe[2] = {-1, -1};
      int out_pipe[2] = {-1, -1};
      if (pipe2(in_pipe, O_CLOEXEC) != 0) {
        return false;
      }
      if (pipe2(out_pipe, O_CLOEXEC) != 0) {
        close(in_pipe[0]);
        close(in_pipe[1]);
        return false;
      }

      posix_spawn_file_actions_t actions;
      posix_spawn_file_actions_init(&actions);
      posix_spawn_file_actions_adddup2(&actions, in_pipe[0], STDIN_FILENO);
      posix_spawn_file_actions_adddup2(&actions, out_pipe[1], STDOUT_FILENO);
      posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

      std::vector<std::string> env_strings;
      for (char **e = environ; e && *e; ++e) {
        env_strings.emplace_back(*e);
      }
      for (auto &extra : session_env_fixups()) {
        env_strings.push_back(std::move(extra));
      }
      std::vector<char *> envp;
      for (auto &s : env_strings) {
        envp.push_back(s.data());
      }
      envp.push_back(nullptr);

      std::vector<std::string> argv_copy = argv;
      std::vector<char *> args;
      for (auto &s : argv_copy) {
        args.push_back(s.data());
      }
      args.push_back(nullptr);

      pid_t pid = -1;
      const int spawn_result = posix_spawnp(&pid, args[0], &actions, nullptr, args.data(), envp.data());
      posix_spawn_file_actions_destroy(&actions);
      close(in_pipe[0]);
      close(out_pipe[1]);
      if (spawn_result != 0) {
        close(in_pipe[1]);
        close(out_pipe[0]);
        return false;
      }
      // Non-blocking ends keep the deadline honest: a tool that stops reading its input or
      // writing its output can never stall this thread past the timeout.
      fcntl(in_pipe[1], F_SETFL, fcntl(in_pipe[1], F_GETFL) | O_NONBLOCK);
      fcntl(out_pipe[0], F_SETFL, fcntl(out_pipe[0], F_GETFL) | O_NONBLOCK);

      // Write stdin, read stdout, both with the same overall deadline.
      std::size_t written = 0;
      bool stdin_open = true;
      bool stdout_open = true;
      if (input.empty()) {
        close(in_pipe[1]);
        stdin_open = false;
      }
      signal(SIGPIPE, SIG_IGN);
      const auto deadline_ms = static_cast<long long>(tool_timeout_ms);
      long long elapsed = 0;
      while ((stdin_open || stdout_open) && elapsed < deadline_ms) {
        pollfd fds[2];
        int count = 0;
        int in_index = -1;
        int out_index = -1;
        if (stdin_open) {
          fds[count] = {in_pipe[1], POLLOUT, 0};
          in_index = count++;
        }
        if (stdout_open) {
          fds[count] = {out_pipe[0], POLLIN, 0};
          out_index = count++;
        }
        const int ready = poll(fds, count, 50);
        elapsed += 50;
        if (ready < 0 && errno != EINTR) {
          break;
        }
        if (in_index >= 0 && (fds[in_index].revents & (POLLOUT | POLLERR | POLLHUP))) {
          const auto n = write(in_pipe[1], input.data() + written, input.size() - written);
          if (n > 0) {
            written += static_cast<std::size_t>(n);
          }
          if (n < 0 && errno != EAGAIN && errno != EINTR) {
            written = input.size();
          }
          if (written >= input.size()) {
            close(in_pipe[1]);
            stdin_open = false;
          }
        }
        if (out_index >= 0 && (fds[out_index].revents & (POLLIN | POLLHUP | POLLERR))) {
          char buffer[4096];
          const auto n = read(out_pipe[0], buffer, sizeof(buffer));
          if (n > 0) {
            if (want_output && output.size() < max_clipboard_bytes) {
              output.append(buffer, static_cast<std::size_t>(n));
            }
          } else if (n == 0 || (errno != EAGAIN && errno != EINTR)) {
            close(out_pipe[0]);
            stdout_open = false;
          }
        }
      }
      if (stdin_open) {
        close(in_pipe[1]);
      }
      if (stdout_open) {
        close(out_pipe[0]);
      }

      // Wait briefly for the exit status; kill a tool that hangs.
      int status = 0;
      bool exited = false;
      for (int i = 0; i < 40; ++i) {
        const auto r = waitpid(pid, &status, WNOHANG);
        if (r == pid) {
          exited = true;
          break;
        }
        usleep(25 * 1000);
      }
      if (!exited) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        return false;
      }
      return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }

    // ---- Klipper over D-Bus -------------------------------------------------------------
    GDBusConnection *session_bus() {
      GError *error = nullptr;
      std::string address = env_or_empty("DBUS_SESSION_BUS_ADDRESS");
      if (address.empty()) {
        address = "unix:path=" + runtime_dir() + "/bus";
      }
      GDBusConnection *connection = g_dbus_connection_new_for_address_sync(
        address.c_str(),
        static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
        nullptr,
        nullptr,
        &error
      );
      if (!connection) {
        BOOST_LOG(debug) << "[clipboard] Cannot reach the session D-Bus at ["sv << address << "]: "sv << (error ? error->message : "unknown error");
        if (error) {
          g_error_free(error);
        }
      }
      return connection;
    }

    bool klipper_get(std::string &out, std::string &why) {
      GDBusConnection *connection = session_bus();
      if (!connection) {
        why = "no session D-Bus";
        return false;
      }
      GError *error = nullptr;
      GVariant *reply = g_dbus_connection_call_sync(
        connection,
        "org.kde.klipper",
        "/klipper",
        "org.kde.klipper.klipper",
        "getClipboardContents",
        nullptr,
        G_VARIANT_TYPE("(s)"),
        G_DBUS_CALL_FLAGS_NONE,
        dbus_timeout_ms,
        nullptr,
        &error
      );
      bool ok = false;
      if (reply) {
        const gchar *text = nullptr;
        g_variant_get(reply, "(&s)", &text);
        if (text) {
          out.assign(text);
          ok = true;
        }
        g_variant_unref(reply);
      } else {
        why = error ? error->message : "unknown error";
        if (error) {
          g_error_free(error);
        }
      }
      g_object_unref(connection);
      return ok;
    }

    bool klipper_set(const std::string &content, std::string &why) {
      GDBusConnection *connection = session_bus();
      if (!connection) {
        why = "no session D-Bus";
        return false;
      }
      GError *error = nullptr;
      GVariant *reply = g_dbus_connection_call_sync(
        connection,
        "org.kde.klipper",
        "/klipper",
        "org.kde.klipper.klipper",
        "setClipboardContents",
        g_variant_new("(s)", content.c_str()),
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        dbus_timeout_ms,
        nullptr,
        &error
      );
      bool ok = reply != nullptr;
      if (reply) {
        g_variant_unref(reply);
      } else {
        why = error ? error->message : "unknown error";
        if (error) {
          g_error_free(error);
        }
      }
      g_object_unref(connection);
      return ok;
    }

    // ---- Session broker ------------------------------------------------------------------
    constexpr const char *session_exec_path = "/usr/libexec/vibeshine/vibepollo-session-exec";
    constexpr std::size_t broker_max_bytes = 32768;

    bool broker_available() {
      return std::getenv("VIBEPOLLO_MACHINE_HOST") != nullptr && access(session_exec_path, X_OK) == 0;
    }

    bool broker_get(std::string &out, std::string &why) {
      std::string raw;
      if (!run_tool({session_exec_path, "clipboard-get"}, "", true, raw)) {
        why = "the session broker could not read Klipper";
        return false;
      }
      GVariant *parsed = g_variant_parse(G_VARIANT_TYPE("(s)"), raw.c_str(), nullptr, nullptr, nullptr);
      if (!parsed) {
        why = "unexpected Klipper reply";
        return false;
      }
      const gchar *text = nullptr;
      g_variant_get(parsed, "(&s)", &text);
      out.assign(text ? text : "");
      g_variant_unref(parsed);
      return true;
    }

    bool broker_set(const std::string &content, std::string &why) {
      if (content.empty() || content.find('\0') != std::string::npos) {
        return true;  // nothing to set; the broker verb rejects empty or NUL-containing text
      }
      if (content.size() > broker_max_bytes) {
        why = "text is larger than the 32 KB the session broker accepts";
        return false;
      }
      static const char digits[] = "0123456789abcdef";
      std::string hex;
      hex.reserve(content.size() * 2);
      for (const unsigned char c : content) {
        hex.push_back(digits[c >> 4]);
        hex.push_back(digits[c & 15]);
      }
      // The text goes through stdin ("-"), never on a command line: argv of every process is
      // readable by all local users through /proc/<pid>/cmdline.
      std::string ignored;
      if (!run_tool({session_exec_path, "clipboard-set", "-"}, hex, false, ignored)) {
        why = "the session broker could not write Klipper";
        return false;
      }
      return true;
    }

    // ---- Backend selection --------------------------------------------------------------
    backend_state_t detect_backend() {
      backend_state_t state;
      const auto desktop = env_or_empty("XDG_CURRENT_DESKTOP");
      const bool is_kde = desktop.find("KDE") != std::string::npos;
      const bool wayland = !env_or_empty("WAYLAND_DISPLAY").empty() || !session_env_fixups().empty();
      const bool x11 = !env_or_empty("DISPLAY").empty();

      std::string klipper_error;
      std::string probe;
      if (broker_available()) {
        std::string broker_why;
        if (broker_get(probe, broker_why)) {
          state.backend = backend_e::broker;
          state.name = "session-broker";
          state.detail = "KDE Klipper in your desktop session, through the Vibebeam session broker";
          return state;
        }
        state.detail = "session broker clipboard unavailable (" + broker_why + "); ";
      }
      if (klipper_get(probe, klipper_error)) {
        state.backend = backend_e::klipper;
        state.name = "klipper";
        state.detail = "KDE Klipper over D-Bus";
        return state;
      }
      state.detail += "Klipper not reachable (" + (klipper_error.empty() ? "no reply"s : klipper_error) + ")";
      if (is_kde) {
        state.detail += "; running Plasma without Klipper? falling back to generic tools";
      }

      if (in_path("wl-paste") && in_path("wl-copy")) {
        std::string out;
        if (run_tool({"wl-paste", "--no-newline", "--type", "text/plain"}, "", true, out) || !out.empty() || wayland) {
          state.backend = backend_e::wl_clipboard;
          state.name = "wl-clipboard";
          state.detail = "wl-copy / wl-paste";
          return state;
        }
      }
      if (x11 && in_path("xclip")) {
        state.backend = backend_e::xclip;
        state.name = "xclip";
        state.detail = "xclip on X11";
        return state;
      }
      state.detail += "; install wl-clipboard (Wayland) or xclip (X11), or start Klipper (KDE)";
      return state;
    }

    /**
     * @brief The selected backend, detected on first use.
     *
     * The session broker is not ready for a short while after boot. If a weaker backend was
     * chosen in that window, look at the broker again (at most every 10 seconds) so the
     * clipboard upgrades to it without a service restart.
     */
    backend_state_t backend() {
      static std::mutex mutex;
      static bool detected = false;
      static backend_state_t state;
      static std::chrono::steady_clock::time_point last_broker_probe {};
      std::lock_guard lock {mutex};
      const auto now = std::chrono::steady_clock::now();
      if (!detected) {
        state = detect_backend();
        detected = true;
        last_broker_probe = now;
        if (state.backend == backend_e::none) {
          BOOST_LOG(warning) << "[clipboard] No clipboard backend available: "sv << state.detail;
        } else {
          BOOST_LOG(info) << "[clipboard] Using "sv << state.name << " ("sv << state.detail << ")"sv;
        }
      } else if (state.backend != backend_e::broker && broker_available() && now - last_broker_probe >= std::chrono::seconds(10)) {
        last_broker_probe = now;
        std::string probe;
        std::string why;
        if (broker_get(probe, why)) {
          state.backend = backend_e::broker;
          state.name = "session-broker";
          state.detail = "KDE Klipper in your desktop session, through the Vibebeam session broker";
          BOOST_LOG(info) << "[clipboard] Switched to "sv << state.name << " ("sv << state.detail << ")"sv;
        }
      }
      return state;
    }
  }  // namespace

  std::string clipboard_backend_name() {
    return backend().name;
  }

  std::string get_clipboard() {
    const auto selected = backend();
    std::string out;
    std::string why;
    bool ok = false;
    switch (selected.backend) {
      case backend_e::broker:
        ok = broker_get(out, why);
        break;
      case backend_e::klipper:
        ok = klipper_get(out, why);
        break;
      case backend_e::wl_clipboard:
        ok = run_tool({"wl-paste", "--no-newline", "--type", "text/plain"}, "", true, out);
        // An empty clipboard makes wl-paste exit non-zero; that is not an error.
        if (!ok && out.empty()) {
          ok = true;
        }
        break;
      case backend_e::xclip:
        ok = run_tool({"xclip", "-selection", "clipboard", "-o"}, "", true, out);
        if (!ok && out.empty()) {
          ok = true;
        }
        break;
      case backend_e::none:
        return "";
    }
    if (!ok) {
      BOOST_LOG(warning) << "[clipboard] Reading with "sv << selected.name << " failed"sv << (why.empty() ? ""s : ": "s + why);
      return "";
    }
    if (out.size() > max_clipboard_bytes) {
      out.resize(max_clipboard_bytes);
    }
    return out;
  }

  bool set_clipboard(const std::string &content) {
    // Reject malformed UTF-8 before GLib's D-Bus variant construction.
    // Backends stop at NUL; only bytes before the first NUL need validation.
    const auto text_length = content.find('\0') == std::string::npos ? content.size() : content.find('\0');
    if (!g_utf8_validate(content.data(), static_cast<gssize>(text_length), nullptr)) {
      BOOST_LOG(warning) << "[clipboard] Refusing to set clipboard: the text is not valid UTF-8 ("sv << content.size() << " bytes)"sv;
      return false;
    }
    const auto selected = backend();
    std::string why;
    std::string ignored;
    bool ok = false;
    switch (selected.backend) {
      case backend_e::broker:
        ok = broker_set(content, why);
        break;
      case backend_e::klipper:
        ok = klipper_set(content, why);
        break;
      case backend_e::wl_clipboard:
        ok = run_tool({"wl-copy", "--type", "text/plain"}, content, false, ignored);
        break;
      case backend_e::xclip:
        ok = run_tool({"xclip", "-selection", "clipboard", "-i"}, content, false, ignored);
        break;
      case backend_e::none:
        return false;
    }
    if (!ok) {
      BOOST_LOG(warning) << "[clipboard] Writing with "sv << selected.name << " failed"sv << (why.empty() ? ""s : ": "s + why);
    }
    return ok;
  }
}  // namespace platf
