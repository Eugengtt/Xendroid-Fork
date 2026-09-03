/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/cpu/backend/a64/a64_code_cache.h"

#include <cstdint>

#include "xenia/base/cvar.h"
#include "xenia/base/platform.h"
#include "xenia/cpu/function.h"
#if XE_PLATFORM_WIN32
#include "xenia/base/platform_win.h"
#endif
#if XE_PLATFORM_xendroid
#include "../aarch64_disasm.h"
#endif
#if defined(__linux__)
#include <unistd.h>

#include <cinttypes>
#include <cstring>
#endif

DEFINE_bool(a64_perf_map, true,
            "Write a simpleperf/perf 'generic JIT symbols' map file "
            "(perf-<pid>.map) mapping generated code to guest function "
            "names, so profilers can attribute JIT samples.",
            "CPU");

namespace xe {
namespace cpu {
namespace backend {
namespace a64 {

A64CodeCache::~A64CodeCache() {
  if (perf_map_file_) {
    std::fclose(perf_map_file_);
  }
}

void A64CodeCache::OnCodePlaced(uint32_t guest_address,
                                GuestFunction* function_info,
                                void* code_execute_address, size_t code_size) {
#if defined(__linux__)
  if (!cvars::a64_perf_map) {
    return;
  }
  std::lock_guard<std::mutex> lock(perf_map_mutex_);
  if (!perf_map_file_) {
    if (perf_map_open_failed_) {
      return;
    }
    // simpleperf looks for the map in the app's data directory
    // (/data/data/<package>/perf-<pid>.map); the package name is the process
    // name up to the ':' (this runs in the :emu process). Fall back to the
    // standalone-program locations.
    char process_name[256] = {0};
    if (FILE* cmdline = std::fopen("/proc/self/cmdline", "rb")) {
      std::fread(process_name, 1, sizeof(process_name) - 1, cmdline);
      std::fclose(cmdline);
    }
    if (char* colon = std::strchr(process_name, ':')) {
      *colon = '\0';
    }
    const int pid = getpid();
    char path[512];
    std::snprintf(path, sizeof(path), "/data/data/%s/perf-%d.map",
                  process_name, pid);
    perf_map_file_ = std::fopen(path, "we");
    if (!perf_map_file_) {
      std::snprintf(path, sizeof(path), "/data/local/tmp/perf-%d.map", pid);
      perf_map_file_ = std::fopen(path, "we");
    }
    if (!perf_map_file_) {
      std::snprintf(path, sizeof(path), "/tmp/perf-%d.map", pid);
      perf_map_file_ = std::fopen(path, "we");
    }
    if (!perf_map_file_) {
      perf_map_open_failed_ = true;
      return;
    }
  }
  const uint64_t address = reinterpret_cast<uint64_t>(code_execute_address);
  if (function_info && !function_info->name().empty()) {
    std::fprintf(perf_map_file_, "0x%" PRIx64 " 0x%zx %s\n", address,
                 code_size, function_info->name().c_str());
  } else if (guest_address) {
    std::fprintf(perf_map_file_, "0x%" PRIx64 " 0x%zx sub_%08X\n", address,
                 code_size, guest_address);
  } else {
    std::fprintf(perf_map_file_, "0x%" PRIx64 " 0x%zx a64_host_code\n",
                 address, code_size);
  }
  std::fflush(perf_map_file_);
#endif
}

void A64CodeCache::FillCode(void* write_address, size_t size) {
  // Fill with BRK #0 (0xD4200000), 4-byte aligned.
  constexpr uint32_t kBrk0 = 0xD4200000;
  auto* p = reinterpret_cast<uint32_t*>(write_address);
  auto* end =
      reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(write_address) + size);
  for (; p < end; ++p) {
    *p = kBrk0;
  }
}

void A64CodeCache::FlushCodeRange(void* address, size_t size) {
#if XE_PLATFORM_WIN32
  FlushInstructionCache(GetCurrentProcess(), address, size);
#else


#if XE_PLATFORM_xendroid
    //XELOGI("ASM:\n{}", aarch64_disasm(reinterpret_cast<uint64_t>(address),reinterpret_cast<uint32_t*>(address),size/4));
#endif
    __builtin___clear_cache(
      reinterpret_cast<char*>(address),
      reinterpret_cast<char*>(static_cast<uint8_t*>(address) + size));
#endif
}

}  // namespace a64
}  // namespace backend
}  // namespace cpu
}  // namespace xe
