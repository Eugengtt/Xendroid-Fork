/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_CPU_CPU_FLAGS_H_
#define XENIA_CPU_CPU_FLAGS_H_
#include <cstdint>
#include <string>

#include "xenia/base/cvar.h"

DECLARE_string(cpu);

DECLARE_string(load_module_map);

DECLARE_string(dump_functions_at);
DECLARE_bool(disassemble_functions);

DECLARE_string(log_guest_calls_at);
DECLARE_string(log_guest_call_fields);
DECLARE_uint32(log_guest_calls_limit);

DECLARE_bool(trace_functions);
DECLARE_bool(trace_function_coverage);
DECLARE_bool(trace_function_references);
DECLARE_bool(trace_function_data);

DECLARE_uint32(cpu_trace_mask);

DECLARE_bool(validate_hir);

DECLARE_uint64(pvr);

// Breakpoints:
DECLARE_uint64(break_on_instruction);
DECLARE_int32(break_condition_gpr);
DECLARE_uint64(break_condition_value);
DECLARE_string(break_condition_op);
DECLARE_bool(break_condition_truncate);

DECLARE_bool(break_on_debugbreak);

namespace xe {
namespace cpu {

// Matches an address against a comma-separated list of hex guest addresses,
// tolerating "0x" and "sub_" prefixes.
bool GuestAddressInList(const std::string& list, uint32_t address);

}  // namespace cpu
}  // namespace xe

#endif  // XENIA_CPU_CPU_FLAGS_H_
