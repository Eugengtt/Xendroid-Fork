/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/cpu/backend/a64/a64_tracers.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "xenia/base/byte_order.h"
#include "xenia/base/logging.h"
#include "xenia/base/vec128.h"
#include "xenia/cpu/cpu_flags.h"
#include "xenia/cpu/ppc/ppc_context.h"
#include "xenia/cpu/processor.h"
#include "xenia/cpu/thread_state.h"
#include "xenia/memory.h"

namespace xe {
namespace cpu {
namespace backend {
namespace a64 {

// Driven by the build options XENIA_ENABLE_ITRACE / XENIA_ENABLE_DTRACE /
// XENIA_ENABLE_FTRACE (xb build --enable-itrace / --enable-dtrace /
// --enable-ftrace).
#if defined(XE_OPTION_TRACE_INSTR) && XE_OPTION_TRACE_INSTR
#define ITRACE 1
#else
#define ITRACE 0
#endif
#if defined(XE_OPTION_TRACE_DATA) && XE_OPTION_TRACE_DATA
#define DTRACE 1
#else
#define DTRACE 0
#endif
#if defined(XE_OPTION_TRACE_FUNC) && XE_OPTION_TRACE_FUNC
#define FTRACE 1
#else
#define FTRACE 0
#endif

#define TARGET_THREAD 0

#define THREAD_MATCH (!TARGET_THREAD || ppc_context->thread_id == TARGET_THREAD)
#define IFLUSH()
#define IPRINT(s)                                              \
  if ((cvars::cpu_trace_mask & TRACING_INSTR) && THREAD_MATCH) \
  xe::logging::AppendLogLine(xe::LogLevel::Info, 't', s, xe::LogSrc::Cpu)
#define DFLUSH()
#define DPRINT(...)                                                          \
  if ((cvars::cpu_trace_mask & TRACING_DATA) && THREAD_MATCH)                \
  xe::logging::AppendLogLineFormat(xe::LogSrc::Cpu, xe::LogLevel::Info, 't', \
                                   __VA_ARGS__)
#define FPRINT(...)                                                          \
  if ((cvars::cpu_trace_mask & TRACING_FUNC) && THREAD_MATCH)                \
  xe::logging::AppendLogLineFormat(xe::LogSrc::Cpu, xe::LogLevel::Info, 't', \
                                   __VA_ARGS__)

// Helper to read float/int lanes from a V128 passed as const uint8_t*.
static inline float v128_f32(const uint8_t* v, int lane) {
  float f;
  std::memcpy(&f, v + lane * 4, 4);
  return f;
}
static inline uint32_t v128_i32(const uint8_t* v, int lane) {
  uint32_t i;
  std::memcpy(&i, v + lane * 4, 4);
  return i;
}

uint32_t GetTracingMode() {
  uint32_t mode = 0;
#if ITRACE
  mode |= TRACING_INSTR;
#endif
#if DTRACE
  mode |= TRACING_DATA;
#endif
#if FTRACE
  mode |= TRACING_FUNC;
#endif
  return mode;
}

static void SetTraceMaskBit(uint32_t bit, bool value) {
  cvars::cpu_trace_mask =
      value ? (cvars::cpu_trace_mask | bit) : (cvars::cpu_trace_mask & ~bit);
}
bool GetTraceInstrEnabled() {
  return (cvars::cpu_trace_mask & TRACING_INSTR) != 0;
}
void SetTraceInstrEnabled(bool value) { SetTraceMaskBit(TRACING_INSTR, value); }
bool GetTraceDataEnabled() {
  return (cvars::cpu_trace_mask & TRACING_DATA) != 0;
}
void SetTraceDataEnabled(bool value) { SetTraceMaskBit(TRACING_DATA, value); }
bool GetTraceFuncEnabled() {
  return (cvars::cpu_trace_mask & TRACING_FUNC) != 0;
}
void SetTraceFuncEnabled(bool value) { SetTraceMaskBit(TRACING_FUNC, value); }

void TraceFunctionEntry(void* raw_context, uint64_t function_address) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  // Guest function entry: caller (lr) and the PPC GPR argument registers
  // r3-r10.
  FPRINT(
      "call {:08X} lr={:08X} r3={:X} r4={:X} r5={:X} r6={:X} r7={:X} r8={:X} "
      "r9={:X} r10={:X}\n",
      static_cast<uint32_t>(function_address),
      static_cast<uint32_t>(ppc_context->lr), ppc_context->r[3],
      ppc_context->r[4], ppc_context->r[5], ppc_context->r[6],
      ppc_context->r[7], ppc_context->r[8], ppc_context->r[9],
      ppc_context->r[10]);
}
void TraceFunctionReturn(void* raw_context, uint64_t function_address) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  // Guest function return value (PPC r3).
  FPRINT("ret  {:08X} = {:X}\n", static_cast<uint32_t>(function_address),
         ppc_context->r[3]);
}

namespace {

struct GuestCallLogSlot {
  std::atomic<uint32_t> address{0};
  std::atomic<uint32_t> hits{0};
  std::atomic<uint64_t> last_value{~uint64_t(0)};
};

constexpr size_t kGuestCallLogSlotCount = 8;
GuestCallLogSlot guest_call_entry_slots[kGuestCallLogSlotCount];
GuestCallLogSlot guest_call_return_slots[kGuestCallLogSlotCount];

// One "rN+off[>off2][:count]" term.
struct GuestCallField {
  std::string label;
  uint32_t reg;
  uint32_t offset;
  bool indirect;
  uint32_t indirect_offset;
  uint32_t words;
};

constexpr uint32_t kMaxGuestCallFieldWords = 8;

const std::vector<GuestCallField>& GetGuestCallFields() {
  static const std::vector<GuestCallField> fields = []() {
    std::vector<GuestCallField> parsed;
    const std::string& list = cvars::log_guest_call_fields;
    for (size_t pos = 0; pos < list.size();) {
      size_t end = list.find(',', pos);
      if (end == std::string::npos) {
        end = list.size();
      }
      const std::string entry = list.substr(pos, end - pos);
      pos = end + 1;
      const char* cursor = entry.c_str();
      while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
      }
      const char* start = cursor;
      if (*cursor == 'r' || *cursor == 'R') {
        ++cursor;
      }
      char* parse_end = nullptr;
      GuestCallField field = {};
      field.reg = uint32_t(std::strtoul(cursor, &parse_end, 10));
      if (parse_end == cursor || field.reg >= 32 || *parse_end != '+') {
        continue;
      }
      cursor = parse_end + 1;
      field.offset = uint32_t(std::strtoul(cursor, &parse_end, 16));
      if (parse_end == cursor) {
        continue;
      }
      cursor = parse_end;
      if (*cursor == '>') {
        field.indirect = true;
        field.indirect_offset = uint32_t(std::strtoul(cursor + 1, &parse_end,
                                                      16));
        cursor = parse_end;
      }
      field.words = 1;
      if (*cursor == ':') {
        field.words = uint32_t(std::strtoul(cursor + 1, &parse_end, 10));
        cursor = parse_end;
        field.words = std::min(std::max(field.words, 1u),
                               kMaxGuestCallFieldWords);
      }
      field.label.assign(start, cursor);
      parsed.push_back(std::move(field));
    }
    return parsed;
  }();
  return fields;
}

bool ReadGuestWord(ppc::PPCContext* ppc_context, uint32_t address,
                   uint32_t* out_value) {
  auto* memory = ppc_context->processor ? ppc_context->processor->memory()
                                        : nullptr;
  if (!memory) {
    return false;
  }
  auto* heap = memory->LookupHeap(address);
  uint32_t protect = 0;
  if (!heap || !heap->QueryProtect(address, &protect) ||
      !(protect & kMemoryProtectRead)) {
    return false;
  }
  *out_value =
      xe::load_and_swap<uint32_t>(ppc_context->virtual_membase + address);
  return true;
}

std::string FormatGuestCallFields(ppc::PPCContext* ppc_context) {
  std::string formatted;
  for (const auto& field : GetGuestCallFields()) {
    uint32_t address =
        static_cast<uint32_t>(ppc_context->r[field.reg]) + field.offset;
    if (field.indirect) {
      uint32_t pointer = 0;
      if (!ReadGuestWord(ppc_context, address, &pointer)) {
        formatted += fmt::format(" [{}]@{:08X}=<unmapped>", field.label,
                                 address);
        continue;
      }
      address = pointer + field.indirect_offset;
    }
    formatted += fmt::format(" [{}]@{:08X}=", field.label, address);
    for (uint32_t word = 0; word < field.words; ++word) {
      uint32_t value = 0;
      if (ReadGuestWord(ppc_context, address + word * 4, &value)) {
        formatted += fmt::format("{}{:08X}", word ? "," : "", value);
      } else {
        formatted += fmt::format("{}<unmapped>", word ? "," : "");
        break;
      }
    }
  }
  return formatted;
}

GuestCallLogSlot* AcquireGuestCallLogSlot(GuestCallLogSlot* slots,
                                          uint32_t address) {
  for (size_t i = 0; i < kGuestCallLogSlotCount; ++i) {
    uint32_t owner = slots[i].address.load(std::memory_order_acquire);
    if (owner == address) {
      return &slots[i];
    }
    if (!owner) {
      uint32_t expected = 0;
      if (slots[i].address.compare_exchange_strong(expected, address,
                                                   std::memory_order_acq_rel) ||
          expected == address) {
        return &slots[i];
      }
    }
  }
  return nullptr;
}

}  // namespace

void LogGuestCallEntry(void* raw_context, uint64_t function_address) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  const uint32_t address = static_cast<uint32_t>(function_address);
  auto* slot = AcquireGuestCallLogSlot(guest_call_entry_slots, address);
  const uint32_t hits =
      slot ? slot->hits.fetch_add(1, std::memory_order_relaxed) + 1 : 1;
  if (cvars::log_guest_calls_limit && hits > cvars::log_guest_calls_limit) {
    return;
  }
  XELOGI(
      "guest call {:08X} #{} thread {:04X} lr={:08X} r3={:08X} r4={:08X} "
      "r5={:08X} r11={:08X}{}",
      address, hits, ppc_context->thread_id,
      static_cast<uint32_t>(ppc_context->lr),
      static_cast<uint32_t>(ppc_context->r[3]),
      static_cast<uint32_t>(ppc_context->r[4]),
      static_cast<uint32_t>(ppc_context->r[5]),
      static_cast<uint32_t>(ppc_context->r[11]),
      FormatGuestCallFields(ppc_context));
}

void LogGuestCallReturn(void* raw_context, uint64_t function_address) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  const uint32_t address = static_cast<uint32_t>(function_address);
  const uint64_t value = ppc_context->r[3];
  auto* slot = AcquireGuestCallLogSlot(guest_call_return_slots, address);
  const uint32_t hits =
      slot ? slot->hits.fetch_add(1, std::memory_order_relaxed) + 1 : 1;
  // A changed return value is the interesting event for a readiness gate, so
  // it outlives the line budget.
  const bool changed =
      slot &&
      slot->last_value.exchange(value, std::memory_order_relaxed) != value;
  if (!changed && cvars::log_guest_calls_limit &&
      hits > cvars::log_guest_calls_limit) {
    return;
  }
  XELOGI("guest ret {:08X} #{} thread {:04X} r3={:08X}", address, hits,
         ppc_context->thread_id, static_cast<uint32_t>(value));
}

void TraceString(void* raw_context, const char* str) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  IPRINT(str);
  IFLUSH();
}

void TraceContextLoadI8(void* raw_context, uint64_t offset, uint8_t value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("{} ({:X}) = ctx i8 +{}\n", (int8_t)value, value, offset);
}
void TraceContextLoadI16(void* raw_context, uint64_t offset, uint16_t value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("{} ({:X}) = ctx i16 +{}\n", (int16_t)value, value, offset);
}
void TraceContextLoadI32(void* raw_context, uint64_t offset, uint32_t value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("{} ({:X}) = ctx i32 +{}\n", (int32_t)value, value, offset);
}
void TraceContextLoadI64(void* raw_context, uint64_t offset, uint64_t value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("{} ({:X}) = ctx i64 +{}\n", (int64_t)value, value, offset);
}
void TraceContextLoadF32(void* raw_context, uint64_t offset, float value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  uint32_t iv;
  std::memcpy(&iv, &value, 4);
  DPRINT("{} ({:X}) = ctx f32 +{}\n", value, iv, offset);
}
void TraceContextLoadF64(void* raw_context, uint64_t offset, double value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  uint64_t iv;
  std::memcpy(&iv, &value, 8);
  DPRINT("{} ({:X}) = ctx f64 +{}\n", value, iv, offset);
}
void TraceContextLoadV128(void* raw_context, uint64_t offset,
                          const uint8_t* value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("[{}, {}, {}, {}] [{:08X}, {:08X}, {:08X}, {:08X}] = ctx v128 +{}\n",
         v128_f32(value, 0), v128_f32(value, 1), v128_f32(value, 2),
         v128_f32(value, 3), v128_i32(value, 0), v128_i32(value, 1),
         v128_i32(value, 2), v128_i32(value, 3), offset);
}

void TraceContextStoreI8(void* raw_context, uint64_t offset, uint8_t value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("ctx i8 +{} = {} ({:X})\n", offset, (int8_t)value, value);
}
void TraceContextStoreI16(void* raw_context, uint64_t offset, uint16_t value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("ctx i16 +{} = {} ({:X})\n", offset, (int16_t)value, value);
}
void TraceContextStoreI32(void* raw_context, uint64_t offset, uint32_t value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("ctx i32 +{} = {} ({:X})\n", offset, (int32_t)value, value);
}
void TraceContextStoreI64(void* raw_context, uint64_t offset, uint64_t value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("ctx i64 +{} = {} ({:X})\n", offset, (int64_t)value, value);
}
void TraceContextStoreF32(void* raw_context, uint64_t offset, float value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  uint32_t iv;
  std::memcpy(&iv, &value, 4);
  DPRINT("ctx f32 +{} = {} ({:X})\n", offset, value, iv);
}
void TraceContextStoreF64(void* raw_context, uint64_t offset, double value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  uint64_t iv;
  std::memcpy(&iv, &value, 8);
  DPRINT("ctx f64 +{} = {} ({:X})\n", offset, value, iv);
}
void TraceContextStoreV128(void* raw_context, uint64_t offset,
                           const uint8_t* value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("ctx v128 +{} = [{}, {}, {}, {}] [{:08X}, {:08X}, {:08X}, {:08X}]\n",
         offset, v128_f32(value, 0), v128_f32(value, 1), v128_f32(value, 2),
         v128_f32(value, 3), v128_i32(value, 0), v128_i32(value, 1),
         v128_i32(value, 2), v128_i32(value, 3));
}

void TraceMemoryLoadI8(void* raw_context, uint32_t address, uint8_t value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("{} ({:X}) = load.i8 {:08X}\n", (int8_t)value, value, address);
}
void TraceMemoryLoadI16(void* raw_context, uint32_t address, uint16_t value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("{} ({:X}) = load.i16 {:08X}\n", (int16_t)value, value, address);
}
void TraceMemoryLoadI32(void* raw_context, uint32_t address, uint32_t value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("{} ({:X}) = load.i32 {:08X}\n", (int32_t)value, value, address);
}
void TraceMemoryLoadI64(void* raw_context, uint32_t address, uint64_t value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("{} ({:X}) = load.i64 {:08X}\n", (int64_t)value, value, address);
}
void TraceMemoryLoadF32(void* raw_context, uint32_t address, float value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  uint32_t iv;
  std::memcpy(&iv, &value, 4);
  DPRINT("{} ({:X}) = load.f32 {:08X}\n", value, iv, address);
}
void TraceMemoryLoadF64(void* raw_context, uint32_t address, double value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  uint64_t iv;
  std::memcpy(&iv, &value, 8);
  DPRINT("{} ({:X}) = load.f64 {:08X}\n", value, iv, address);
}
void TraceMemoryLoadV128(void* raw_context, uint32_t address,
                         const uint8_t* value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT(
      "[{}, {}, {}, {}] [{:08X}, {:08X}, {:08X}, {:08X}] = load.v128 {:08X}\n",
      v128_f32(value, 0), v128_f32(value, 1), v128_f32(value, 2),
      v128_f32(value, 3), v128_i32(value, 0), v128_i32(value, 1),
      v128_i32(value, 2), v128_i32(value, 3), address);
}

void TraceMemoryStoreI8(void* raw_context, uint32_t address, uint8_t value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("store.i8 {:08X} = {} ({:X})\n", address, (int8_t)value, value);
}
void TraceMemoryStoreI16(void* raw_context, uint32_t address, uint16_t value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("store.i16 {:08X} = {} ({:X})\n", address, (int16_t)value, value);
}
void TraceMemoryStoreI32(void* raw_context, uint32_t address, uint32_t value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("store.i32 {:08X} = {} ({:X})\n", address, (int32_t)value, value);
}
void TraceMemoryStoreI64(void* raw_context, uint32_t address, uint64_t value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("store.i64 {:08X} = {} ({:X})\n", address, (int64_t)value, value);
}
void TraceMemoryStoreF32(void* raw_context, uint32_t address, float value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  uint32_t iv;
  std::memcpy(&iv, &value, 4);
  DPRINT("store.f32 {:08X} = {} ({:X})\n", address, value, iv);
}
void TraceMemoryStoreF64(void* raw_context, uint32_t address, double value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  uint64_t iv;
  std::memcpy(&iv, &value, 8);
  DPRINT("store.f64 {:08X} = {} ({:X})\n", address, value, iv);
}
void TraceMemoryStoreV128(void* raw_context, uint32_t address,
                          const uint8_t* value) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT(
      "store.v128 {:08X} = [{}, {}, {}, {}] [{:08X}, {:08X}, {:08X}, {:08X}]\n",
      address, v128_f32(value, 0), v128_f32(value, 1), v128_f32(value, 2),
      v128_f32(value, 3), v128_i32(value, 0), v128_i32(value, 1),
      v128_i32(value, 2), v128_i32(value, 3));
}

void TraceMemset(void* raw_context, uint32_t address, uint8_t value,
                 uint32_t length) {
  auto ppc_context = reinterpret_cast<ppc::PPCContext*>(raw_context);
  DPRINT("memset {:08X}-{:08X} ({}) = {:02X}", address, address + length,
         length, value);
}

}  // namespace a64
}  // namespace backend
}  // namespace cpu
}  // namespace xe
