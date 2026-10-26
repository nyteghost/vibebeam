/**
 * @file src/two_factor.cpp
 * @brief Optional two-factor (TOTP) check for starting or resuming a stream.
 */
#include "two_factor.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

namespace two_factor {

  namespace {
    constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    constexpr int kStepSeconds = 30;
    constexpr int kMaxFailures = 5;
    constexpr int kLockSeconds = 300;
    constexpr int kBackupCodeCount = 8;
    constexpr int kPendingSeconds = 600;

    std::mutex g_mutex;
    std::filesystem::path g_state_path_override;
    std::int64_t g_fixed_clock = 0;

    struct limiter_t {
      int failures = 0;
      std::int64_t locked_until = 0;
    };
    std::map<std::string, limiter_t> g_limiters;
    std::map<std::string, std::int64_t> g_grace;

    struct pending_t {
      std::vector<std::uint8_t> secret;
      std::int64_t created = 0;
    };
    std::optional<pending_t> g_pending;

    struct state_t {
      bool enabled = false;
      bool broken = false;  // the file exists but cannot be used: treat 2FA as on and refuse
      std::vector<std::uint8_t> secret;
      std::uint64_t last_step = 0;
      std::vector<std::string> backup_hashes;  // hex SHA-256
    };

    std::int64_t now_seconds() {
      if (g_fixed_clock != 0) {
        return g_fixed_clock;
      }
      return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::filesystem::path state_path() {
      if (!g_state_path_override.empty()) {
        return g_state_path_override;
      }
      return {};  // Not initialised: the feature stays off.
    }

    std::string hex(const unsigned char *data, std::size_t len) {
      static constexpr char digits[] = "0123456789abcdef";
      std::string out;
      out.reserve(len * 2);
      for (std::size_t i = 0; i < len; ++i) {
        out.push_back(digits[data[i] >> 4]);
        out.push_back(digits[data[i] & 0x0f]);
      }
      return out;
    }

    std::string sha256_hex(std::string_view text) {
      unsigned char digest[SHA256_DIGEST_LENGTH];
      SHA256(reinterpret_cast<const unsigned char *>(text.data()), text.size(), digest);
      return hex(digest, sizeof(digest));
    }

    bool equal_constant_time(std::string_view a, std::string_view b) {
      return a.size() == b.size() && CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
    }

    state_t load_state() {
      state_t state;
      if (state_path().empty()) {
        return state;
      }
      std::error_code exists_ec;
      if (!std::filesystem::exists(state_path(), exists_ec)) {
        return state;  // No file: 2FA is off.
      }
      std::ifstream in(state_path(), std::ios::binary);
      if (!in) {
        state.enabled = state.broken = true;  // Present but unreadable: fail closed.
        return state;
      }
      try {
        const auto json = nlohmann::json::parse(in);
        if (!json.value("enabled", false)) {
          return state;
        }
        if (!base32_decode(json.value("secret", ""), state.secret) || state.secret.size() < 16) {
          state.enabled = state.broken = true;
          return state;
        }
        state.enabled = true;
        state.last_step = json.value("last_step", static_cast<std::uint64_t>(0));
        if (json.contains("backup") && json["backup"].is_array()) {
          for (const auto &item : json["backup"]) {
            if (item.is_string()) {
              state.backup_hashes.push_back(item.get<std::string>());
            }
          }
        }
      } catch (...) {
        state = {};
        state.enabled = state.broken = true;  // Corrupt file: fail closed.
      }
      return state;
    }

    bool save_state(const state_t &state) {
      nlohmann::json json;
      json["enabled"] = state.enabled;
      json["secret"] = base32_encode(state.secret);
      json["last_step"] = state.last_step;
      json["backup"] = state.backup_hashes;

      const auto path = state_path();
      if (path.empty()) {
        return false;
      }
      auto tmp = path;
      tmp += ".tmp";
      {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
          return false;
        }
        std::error_code ec;
        std::filesystem::permissions(tmp, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace, ec);
        out << json.dump();
        if (!out) {
          return false;
        }
      }
      std::error_code ec;
      std::filesystem::rename(tmp, path, ec);
      return !ec;
    }

    bool remove_state() {
      std::error_code ec;
      std::filesystem::remove(state_path(), ec);
      return !ec;
    }

    std::string clean(std::string_view code) {
      std::string out;
      for (char c : code) {
        if (c == ' ' || c == '-') {
          continue;
        }
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
      }
      return out;
    }

    bool is_six_digits(const std::string &s) {
      return s.size() == 6 && std::all_of(s.begin(), s.end(), [](char c) {
        return c >= '0' && c <= '9';
      });
    }

    /// Check a current-code or backup-code; updates `state` (replay/backup consumption). Caller saves it.
    bool verify_code(state_t &state, std::string_view raw, bool allow_backup) {
      const std::string code = clean(raw);
      if (is_six_digits(code)) {
        const auto now_step = static_cast<std::uint64_t>(now_seconds() / kStepSeconds);
        bool matched = false;
        std::uint64_t matched_step = 0;
        for (std::int64_t offset = -1; offset <= 1; ++offset) {
          const auto step = now_step + offset;
          // Compare every window so timing does not reveal which one matched.
          if (equal_constant_time(totp_code(state.secret, step), code) && step > state.last_step) {
            if (!matched || step > matched_step) {
              matched_step = step;
            }
            matched = true;
          }
        }
        if (matched) {
          state.last_step = matched_step;
          return true;
        }
        return false;
      }
      if (allow_backup && code.size() == 10) {
        const auto hash = sha256_hex(code);
        bool found = false;
        std::size_t index = 0;
        for (std::size_t i = 0; i < state.backup_hashes.size(); ++i) {
          if (equal_constant_time(state.backup_hashes[i], hash)) {
            found = true;
            index = i;
          }
        }
        if (found) {
          state.backup_hashes.erase(state.backup_hashes.begin() + static_cast<std::ptrdiff_t>(index));
          return true;
        }
      }
      return false;
    }

    bool locked(const std::string &key, std::int64_t now) {
      const auto it = g_limiters.find(key);
      return it != g_limiters.end() && it->second.locked_until > now;
    }

    void note_failure(const std::string &key, std::int64_t now) {
      auto &entry = g_limiters[key];
      if (++entry.failures >= kMaxFailures) {
        entry.failures = 0;
        entry.locked_until = now + kLockSeconds;
      }
    }

    void note_success(const std::string &key) {
      g_limiters.erase(key);
    }

    std::string random_backup_code() {
      std::array<std::uint8_t, 10> bytes {};
      RAND_bytes(bytes.data(), static_cast<int>(bytes.size()));
      std::string out;
      for (auto b : bytes) {
        out.push_back(kAlphabet[b & 31]);
      }
      return out;
    }

    std::string display_backup_code(const std::string &code) {
      return code.substr(0, 5) + "-" + code.substr(5);
    }
  }  // namespace

  std::string base32_encode(const std::vector<std::uint8_t> &data) {
    std::string out;
    std::uint32_t buffer = 0;
    int bits = 0;
    for (auto byte : data) {
      buffer = (buffer << 8) | byte;
      bits += 8;
      while (bits >= 5) {
        out.push_back(kAlphabet[(buffer >> (bits - 5)) & 31]);
        bits -= 5;
      }
    }
    if (bits > 0) {
      out.push_back(kAlphabet[(buffer << (5 - bits)) & 31]);
    }
    return out;
  }

  bool base32_decode(std::string_view text, std::vector<std::uint8_t> &out) {
    out.clear();
    std::uint32_t buffer = 0;
    int bits = 0;
    for (char raw : text) {
      if (raw == '=' || raw == ' ') {
        continue;
      }
      const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
      const char *pos = std::strchr(kAlphabet, c);
      if (!pos || c == '\0') {
        return false;
      }
      buffer = (buffer << 5) | static_cast<std::uint32_t>(pos - kAlphabet);
      bits += 5;
      if (bits >= 8) {
        out.push_back(static_cast<std::uint8_t>((buffer >> (bits - 8)) & 0xff));
        bits -= 8;
      }
    }
    return true;
  }

  std::string totp_code(const std::vector<std::uint8_t> &secret, std::uint64_t step, int digits) {
    unsigned char message[8];
    for (int i = 7; i >= 0; --i) {
      message[i] = static_cast<unsigned char>(step & 0xff);
      step >>= 8;
    }
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    HMAC(EVP_sha1(), secret.data(), static_cast<int>(secret.size()), message, sizeof(message), digest, &digest_len);
    if (digest_len < 20) {
      return {};
    }
    const int offset = digest[digest_len - 1] & 0x0f;
    const std::uint32_t binary = ((digest[offset] & 0x7f) << 24) | (digest[offset + 1] << 16) | (digest[offset + 2] << 8) | digest[offset + 3];
    std::uint32_t modulus = 1;
    for (int i = 0; i < digits; ++i) {
      modulus *= 10;
    }
    std::string out = std::to_string(binary % modulus);
    while (static_cast<int>(out.size()) < digits) {
      out.insert(out.begin(), '0');
    }
    return out;
  }

  bool enabled() {
    std::lock_guard lock {g_mutex};
    return load_state().enabled;
  }

  const char *status_message(result_e result) {
    switch (result) {
      case result_e::required:
        return "Two-factor code required";
      case result_e::invalid:
        return "Two-factor code was not accepted";
      case result_e::locked:
        return "Too many wrong two-factor codes; try again in a few minutes";
      case result_e::unavailable:
        return "Two-factor is on but its settings could not be read. Ask the host owner to check the host log";
      case result_e::ok:
        break;
    }
    return "OK";
  }

  int status_code(result_e result) {
    switch (result) {
      case result_e::required:
        return 470;
      case result_e::invalid:
        return 471;
      case result_e::locked:
        return 472;
      case result_e::unavailable:
        return 473;
      case result_e::ok:
        break;
    }
    return 200;
  }

  result_e check(const std::string &client_uuid, std::string_view code, bool exempt) {
    std::lock_guard lock {g_mutex};
    auto state = load_state();
    if (!state.enabled || exempt) {
      return result_e::ok;
    }
    if (state.broken) {
      return result_e::unavailable;
    }

    const auto now = now_seconds();
    for (auto it = g_grace.begin(); it != g_grace.end();) {
      it = it->second + kGraceSeconds <= now ? g_grace.erase(it) : std::next(it);
    }
    if (g_grace.count(client_uuid)) {
      return result_e::ok;
    }
    if (code.empty()) {
      return result_e::required;
    }
    if (locked(client_uuid, now)) {
      return result_e::locked;
    }
    if (verify_code(state, code, true) && save_state(state)) {
      note_success(client_uuid);
      g_grace[client_uuid] = now;
      return result_e::ok;
    }
    note_failure(client_uuid, now);
    return locked(client_uuid, now) ? result_e::locked : result_e::invalid;
  }

  status_t status() {
    std::lock_guard lock {g_mutex};
    const auto state = load_state();
    const auto now = now_seconds();
    const bool pending = g_pending && g_pending->created + kPendingSeconds > now;
    return {state.enabled, pending, state.backup_hashes.size()};
  }

  nlohmann::json begin_setup(const std::string &account_label) {
    std::lock_guard lock {g_mutex};
    pending_t pending;
    pending.secret.resize(20);
    RAND_bytes(pending.secret.data(), static_cast<int>(pending.secret.size()));
    pending.created = now_seconds();
    const auto secret = base32_encode(pending.secret);
    g_pending = std::move(pending);

    std::string label;
    for (char c : account_label) {
      label += (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.') ? c : '-';
    }
    if (label.empty()) {
      label = "host";
    }
    nlohmann::json out;
    out["secret"] = secret;
    out["uri"] = "otpauth://totp/Vibebeam:" + label + "?secret=" + secret + "&issuer=Vibebeam&algorithm=SHA1&digits=6&period=30";
    return out;
  }

  std::optional<std::vector<std::string>> confirm_setup(std::string_view code) {
    std::lock_guard lock {g_mutex};
    const auto now = now_seconds();
    if (!g_pending || g_pending->created + kPendingSeconds <= now || locked("setup", now)) {
      return std::nullopt;
    }
    state_t state;
    state.enabled = true;
    state.secret = g_pending->secret;
    if (!verify_code(state, code, false)) {
      note_failure("setup", now);
      return std::nullopt;
    }
    std::vector<std::string> shown;
    for (int i = 0; i < kBackupCodeCount; ++i) {
      const auto raw = random_backup_code();
      state.backup_hashes.push_back(sha256_hex(raw));
      shown.push_back(display_backup_code(raw));
    }
    if (!save_state(state)) {
      return std::nullopt;
    }
    g_pending.reset();
    note_success("setup");
    g_grace.clear();
    return shown;
  }

  result_e disable(std::string_view code) {
    std::lock_guard lock {g_mutex};
    auto state = load_state();
    if (!state.enabled) {
      return result_e::ok;
    }
    if (state.broken) {
      return result_e::unavailable;
    }
    const auto now = now_seconds();
    if (locked("disable", now)) {
      return result_e::locked;
    }
    if (verify_code(state, code, true) && remove_state()) {
      note_success("disable");
      g_grace.clear();
      return result_e::ok;
    }
    note_failure("disable", now);
    return locked("disable", now) ? result_e::locked : result_e::invalid;
  }

  void set_state_path(const std::filesystem::path &path) {
    std::lock_guard lock {g_mutex};
    g_state_path_override = path;
  }

  bool reset_state() {
    std::lock_guard lock {g_mutex};
    g_limiters.clear();
    g_grace.clear();
    g_pending.reset();
    const auto path = state_path();
    if (path.empty()) {
      return false;
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return !ec && !std::filesystem::exists(path, ec);
  }

  void set_clock_for_tests(std::int64_t unix_seconds) {
    std::lock_guard lock {g_mutex};
    g_fixed_clock = unix_seconds;
  }

  void reset_runtime_for_tests() {
    std::lock_guard lock {g_mutex};
    g_limiters.clear();
    g_grace.clear();
    g_pending.reset();
  }

}  // namespace two_factor
