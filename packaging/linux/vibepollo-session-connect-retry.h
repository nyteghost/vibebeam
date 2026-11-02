#pragma once

#include <errno.h>
#include <stdbool.h>

// No request has been sent at this point. Never replay a request after a
// response reset: its mutation may already have happened in the old session.
static inline int session_connect_with_retry(bool audio_operation,
                                            int (*connect_once)(void),
                                            void (*backoff)(void)) {
  for (unsigned int attempt = 0;; ++attempt) {
    const int fd = connect_once();
    if (fd >= 0 || !audio_operation || attempt >= 3 ||
        (errno != ECONNREFUSED && errno != ENOENT)) return fd;
    backoff();
  }
}
