#pragma once

// Copy exactly the number of bytes already accepted by the HTTP request parser.
// A premature EOF must fail: otherwise a truncated upload can be reported as
// successfully received. The caller owns the FILE and must check fclose() and
// remove any partially written file on failure.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <streambuf>

namespace upload_io {
  inline bool copy_exact(std::streambuf *source, std::FILE *destination, std::size_t expected_bytes) {
    if (!source || !destination) {
      return false;
    }

    std::array<char, 64 * 1024> chunk {};
    std::size_t remaining = expected_bytes;
    while (remaining != 0) {
      const auto requested = static_cast<std::streamsize>(std::min(remaining, chunk.size()));
      const auto received = source->sgetn(chunk.data(), requested);
      if (received <= 0) {
        return false;  // Unexpected EOF before the full request body was copied.
      }

      const auto bytes = static_cast<std::size_t>(received);
      if (std::fwrite(chunk.data(), 1, bytes, destination) != bytes) {
        return false;
      }
      remaining -= bytes;
    }
    return true;
  }
}  // namespace upload_io
