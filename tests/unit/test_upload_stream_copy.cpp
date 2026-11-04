#include "src/upload_stream_copy.h"

#include <cstdio>
#include <ios>
#include <sstream>
#include <string>
#include <gtest/gtest.h>

namespace {
  struct temporary_file_t {
    std::FILE *file = std::tmpfile();
    ~temporary_file_t() {
      if (file) {
        std::fclose(file);
      }
    }
  };

  TEST(UploadStreamCopy, CopiesExactlyOneChunk) {
    const std::string body = "hello upload";
    std::stringbuf input(body, std::ios::in);
    temporary_file_t out;
    ASSERT_NE(out.file, nullptr);
    ASSERT_TRUE(upload_io::copy_exact(&input, out.file, body.size()));
    ASSERT_EQ(std::fflush(out.file), 0);
    std::rewind(out.file);
    std::string stored(body.size(), '\0');
    EXPECT_EQ(std::fread(stored.data(), 1, stored.size(), out.file), body.size());
    EXPECT_EQ(stored, body);
  }

  TEST(UploadStreamCopy, CopiesAcrossChunkBoundaries) {
    std::string body(64 * 1024 * 2 + 17, 'x');
    body[65'536] = 'A';
    body.back() = 'Z';
    std::stringbuf input(body, std::ios::in);
    temporary_file_t out;
    ASSERT_NE(out.file, nullptr);
    ASSERT_TRUE(upload_io::copy_exact(&input, out.file, body.size()));
    ASSERT_EQ(std::fflush(out.file), 0);
    std::rewind(out.file);
    std::string stored(body.size(), '\0');
    EXPECT_EQ(std::fread(stored.data(), 1, stored.size(), out.file), body.size());
    EXPECT_EQ(stored, body);
  }

  TEST(UploadStreamCopy, RejectsTruncatedBody) {
    const std::string body = "partial";
    std::stringbuf input(body, std::ios::in);
    temporary_file_t out;
    ASSERT_NE(out.file, nullptr);
    EXPECT_FALSE(upload_io::copy_exact(&input, out.file, body.size() + 1));
    // The caller must delete this partial file; it must not return success.
    ASSERT_EQ(std::fflush(out.file), 0);
    EXPECT_EQ(std::ftell(out.file), static_cast<long>(body.size()));
  }

  TEST(UploadStreamCopy, AcceptsEmptyBody) {
    std::stringbuf input("", std::ios::in);
    temporary_file_t out;
    ASSERT_NE(out.file, nullptr);
    EXPECT_TRUE(upload_io::copy_exact(&input, out.file, 0));
    EXPECT_EQ(std::ftell(out.file), 0);
  }

  TEST(UploadStreamCopy, RejectsNullHandles) {
    std::stringbuf input("data", std::ios::in);
    temporary_file_t out;
    ASSERT_NE(out.file, nullptr);
    EXPECT_FALSE(upload_io::copy_exact(nullptr, out.file, 4));
    EXPECT_FALSE(upload_io::copy_exact(&input, nullptr, 4));
  }
}  // namespace
