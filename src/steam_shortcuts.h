/** @file src/steam_shortcuts.h
 * Bounded reader for Steam's binary KeyValues shortcuts.vdf (not appinfo).
 * Only display metadata leaves this reader. Exe/LaunchOptions are never run.
 */
#pragma once

#include "steam_game_id.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace platf::steam::shortcuts {
  inline constexpr std::size_t max_file_bytes = 16U * 1024U * 1024U;
  inline constexpr std::size_t max_entries = 4096;

  struct entry_t {
    std::uint32_t app_id;
    std::string name;
    std::uint32_t last_played;
  };

  namespace detail {
    struct node_t {
      std::uint8_t type = 0;
      std::string text;
      std::uint32_t integer = 0;
      std::map<std::string, node_t> children;

      const node_t *field(const char *name, std::uint8_t expected_type) const {
        const auto it = children.find(name);
        return it != children.end() && it->second.type == expected_type ? &it->second : nullptr;
      }
    };

    class reader {
    public:
      explicit reader(std::string_view data): data_ {data} {}

      node_t object(unsigned depth = 0) {
        if (depth > 8) fail();
        node_t out;
        while (true) {
          const auto type = byte();
          if (type == 8) return out;
          if (++nodes_ > 131072) fail();
          auto key = string(256);
          std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : static_cast<char>(c);
          });
          node_t value;
          switch (type) {
            case 0: value = object(depth + 1); break;
            case 1: value.text = string(65536); break;
            case 2: value.integer = u32(); break;
            // Skip known, unused scalar types without interpreting them as
            // IDs or strings. Unknown types invalidate the whole file.
            case 3:
            case 4:
            case 6: skip(4); break;
            case 7:
            case 10: skip(8); break;
            default: fail();
          }
          value.type = type;
          if (!out.children.emplace(std::move(key), std::move(value)).second) fail();
        }
      }

      bool finished() const { return offset_ == data_.size(); }

    private:
      [[noreturn]] static void fail() { throw std::invalid_argument("Invalid Steam shortcuts metadata"); }
      std::uint8_t byte() {
        if (offset_ == data_.size()) fail();
        return static_cast<std::uint8_t>(data_[offset_++]);
      }
      void skip(std::size_t length) {
        if (length > data_.size() - offset_) fail();
        offset_ += length;
      }
      std::uint32_t u32() {
        std::uint32_t result = 0;
        for (unsigned shift = 0; shift < 32; shift += 8) result |= std::uint32_t {byte()} << shift;
        return result;
      }
      std::string string(std::size_t maximum) {
        const auto end = data_.find('\0', offset_);
        if (end == std::string_view::npos || end - offset_ > maximum) fail();
        auto result = std::string {data_.substr(offset_, end - offset_)};
        offset_ = end + 1;
        return result;
      }
      std::string_view data_;
      std::size_t offset_ = 0;
      std::size_t nodes_ = 0;
    };

    inline bool valid_name(std::string_view name) {
      if (name.empty() || name.size() > 512) return false;
      // Validate UTF-8 before a single bad shortcut can poison the bounded
      // JSON catalog returned by the desktop-user scanner.
      for (std::size_t i = 0; i < name.size();) {
        const auto first = static_cast<unsigned char>(name[i++]);
        if (first < 0x20 || first == 0x7f) return false;
        if (first < 0x80) continue;
        const unsigned count = first >= 0xc2 && first <= 0xdf ? 1 :
                               first >= 0xe0 && first <= 0xef ? 2 :
                               first >= 0xf0 && first <= 0xf4 ? 3 : 0;
        if (!count || count > name.size() - i) return false;
        const auto second = static_cast<unsigned char>(name[i]);
        if ((first == 0xe0 && second < 0xa0) || (first == 0xed && second >= 0xa0) ||
            (first == 0xf0 && second < 0x90) || (first == 0xf4 && second >= 0x90)) return false;
        for (unsigned j = 0; j < count; ++j) {
          if ((static_cast<unsigned char>(name[i++]) & 0xc0) != 0x80) return false;
        }
      }
      return true;
    }
  }  // namespace detail

  inline std::optional<std::vector<entry_t>> parse(std::string_view contents) {
    if (contents.empty() || contents.size() > max_file_bytes) return std::nullopt;
    try {
      detail::reader reader {contents};
      const auto root = reader.object();
      const auto *shortcuts = root.field("shortcuts", 0);
      if (!reader.finished() || root.children.size() != 1 || !shortcuts ||
          shortcuts->children.size() > max_entries) return std::nullopt;
      std::vector<entry_t> entries;
      for (const auto &[index, node] : shortcuts->children) {
        if (node.type != 0 || index.empty() || !std::all_of(index.begin(), index.end(), [](char c) {
              return c >= '0' && c <= '9';
            })) continue;
        const auto *id = node.field("appid", 2);
        const auto *name = node.field("appname", 1);
        const auto *exe = node.field("exe", 1);
        const auto *hidden = node.field("ishidden", 2);
        const auto *played = node.field("lastplaytime", 2);
        // Use the ID Steam persisted, never a guessed CRC of name/path.
        if (!id || !steam_is_shortcut_id(id->integer) || !name || !detail::valid_name(name->text) ||
            !exe || exe->text.empty() || (hidden && hidden->integer != 0) ||
            (node.children.contains("ishidden") && !hidden)) continue;
        entries.push_back({id->integer, name->text, played ? played->integer : 0});
      }
      return entries;
    } catch (const std::invalid_argument &) {
      // Steam can rewrite the file during a scan. Publish no partial file.
      return std::nullopt;
    }
  }
}  // namespace platf::steam::shortcuts
