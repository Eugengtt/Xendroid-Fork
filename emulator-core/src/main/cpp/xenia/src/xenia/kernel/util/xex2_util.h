/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_KERNEL_UTIL_XEX2_UTIL_H_
#define XENIA_KERNEL_UTIL_XEX2_UTIL_H_

#include <cstdint>

#include "xenia/kernel/util/xex2_info.h"

namespace xe {

// Special case: if key & 0xFF == 0x00, returns the value in *out_ptr as
// uint32_t, not a pointer.
inline bool GetXexOptHeader(const xex2_header* header, xex2_header_keys key,
                            void** out_ptr) {
  if (!header || !out_ptr) {
    return false;
  }

  for (uint32_t i = 0; i < header->header_count; i++) {
    const xex2_opt_header& opt_header = header->headers[i];
    if (opt_header.key == key) {
      switch (key & 0xFF) {
        case 0x00:
          *reinterpret_cast<uint32_t*>(out_ptr) =
              static_cast<uint32_t>(opt_header.value);
          break;
        case 0x01:
          *out_ptr = const_cast<void*>(
              reinterpret_cast<const void*>(&opt_header.value));
          break;
        default:
          *out_ptr =
              reinterpret_cast<void*>(uintptr_t(header) + opt_header.offset);
          break;
      }
      return true;
    }
  }
  return false;
}

template <typename T>
inline bool GetXexOptHeader(const xex2_header* header, xex2_header_keys key,
                            T* out_ptr) {
  return GetXexOptHeader(header, key, reinterpret_cast<void**>(out_ptr));
}

}  // namespace xe

#endif  // XENIA_KERNEL_UTIL_XEX2_UTIL_H_
