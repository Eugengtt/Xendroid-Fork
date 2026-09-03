/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_APP_TITLE_ID_UTIL_H_
#define XENIA_APP_TITLE_ID_UTIL_H_

#include <cstddef>
#include <cstdint>

namespace xe {
namespace app {

// Parses an exactly-8-char hex title id. Returns 0 on any wrong length or
// non-hex character. Used by the embedded title databases.
inline uint32_t ParseHexTitleId(const char* hex, size_t len) {
  if (len != 8) {
    return 0;
  }
  uint32_t v = 0;
  for (size_t i = 0; i < len; ++i) {
    char c = hex[i];
    uint32_t d;
    if (c >= '0' && c <= '9') {
      d = static_cast<uint32_t>(c - '0');
    } else if (c >= 'a' && c <= 'f') {
      d = static_cast<uint32_t>(c - 'a' + 10);
    } else if (c >= 'A' && c <= 'F') {
      d = static_cast<uint32_t>(c - 'A' + 10);
    } else {
      return 0;
    }
    v = (v << 4) | d;
  }
  return v;
}

}  // namespace app
}  // namespace xe

#endif  // XENIA_APP_TITLE_ID_UTIL_H_
