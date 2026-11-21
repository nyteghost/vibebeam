#pragma once

#include <cstdint>
#include <vector>

namespace egl {
  enum class modifier_support_e { unknown, absent, texture_2d, external_only };

  // The diagnostic adapter supplies EGL-native scalar types. Driver results
  // must be bounded before allocating or reading either returned array.
  template<class Modifier, class Flag, class Query>
  modifier_support_e query_modifier_support(std::uint64_t modifier, Query query) {
    int count = 0;
    if (!query(0, nullptr, nullptr, &count) || count <= 0 || count > 4096) return modifier_support_e::unknown;
    std::vector<Modifier> modifiers(count);
    std::vector<Flag> external_only(count);
    const int capacity = count;
    if (!query(capacity, modifiers.data(), external_only.data(), &count) || count < 0 || count > capacity) return modifier_support_e::unknown;
    for (int index = 0; index < count; ++index) {
      if (modifiers[index] == modifier) return external_only[index] ? modifier_support_e::external_only : modifier_support_e::texture_2d;
    }
    return modifier_support_e::absent;
  }
}
