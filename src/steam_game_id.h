/** @file src/steam_game_id.h
 * Shared C/C++ conversion for Steam's stored non-Steam shortcut IDs.
 */
#pragma once

#include <stdint.h>

// Steam reserves the high bit of the stored 32-bit ID for shortcuts. Keep
// that ID in catalogs/API requests; only the Steam URI needs a 64-bit GameID.
static inline int steam_is_shortcut_id(uint32_t app_id) {
  return (app_id & UINT32_C(0x80000000)) != 0;
}

static inline uint64_t steam_launch_game_id(uint32_t app_id) {
  return steam_is_shortcut_id(app_id) ?
           ((uint64_t) app_id << 32) | UINT64_C(0x02000000) : app_id;
}
