/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/cpu/testing/util.h"

#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "xenia/cpu/ppc/ppc_emit.h"

using namespace xe::cpu::hir;
using namespace xe::cpu;
using namespace xe::cpu::testing;
using xe::cpu::ppc::PPCContext;

static void SetF64Bits(PPCContext* ctx, int reg, uint64_t bits) {
  std::memcpy(&ctx->f[reg], &bits, sizeof(bits));
}
static uint64_t GetF64Bits(PPCContext* ctx, int reg) {
  uint64_t bits;
  std::memcpy(&bits, &ctx->f[reg], sizeof(bits));
  return bits;
}
static bool IsDoubleNaN(uint64_t bits) {
  return (bits & 0x7FF0000000000000ull) == 0x7FF0000000000000ull &&
         (bits & 0x000FFFFFFFFFFFFFull) != 0;
}

// double -> single (stfs). PowerPC keeps the signaling bit; the host convert
// would quiet it. The trailing inf case is the documented payload-truncation
// behavior (top 23 fraction bits zero -> single fraction zero -> infinity),
// which matches Xenon's stfs.
TEST_CASE("PACK_SINGLE_KEEP_NAN", "[instr]") {
  TestFunction test([](HIRBuilder& b) {
    Value* sbits = ppc::PackSingleKeepNaN(b, LoadFPR(b, 1));
    StoreGPR(b, 3, b.ZeroExtend(sbits, INT64_TYPE));
    b.Return();
  });

  // {double input bits, expected single bits}
  const std::vector<std::pair<uint64_t, uint32_t>> cases = {
      {0x3FF0000000000000ull, 0x3F800000u},  // 1.0
      {0xC000000000000000ull, 0xC0000000u},  // -2.0
      {0x7FF0000000000000ull, 0x7F800000u},  // +inf
      {0xFFF0000000000000ull, 0xFF800000u},  // -inf
      {0x7FF8000000000000ull, 0x7FC00000u},  // qnan stays quiet
      {0x7FF4000000000000ull, 0x7FA00000u},  // snan stays signaling
      {0xFFF4000000000000ull, 0xFFA00000u},  // -snan stays signaling
      {0x7FF0000000000001ull, 0x7F800000u},  // snan payload truncated -> inf
  };
  for (const auto& c : cases) {
    INFO("double 0x" << std::hex << c.first);
    test.Run([&](PPCContext* ctx) { SetF64Bits(ctx, 1, c.first); },
             [&](PPCContext* ctx) {
               REQUIRE(static_cast<uint32_t>(ctx->r[3]) == c.second);
             });
  }
}

// single -> double (lfs). Widening never drops NaN-ness, so the only fixup is
// keeping the signaling bit the host convert would otherwise quiet.
TEST_CASE("UNPACK_SINGLE_KEEP_NAN", "[instr]") {
  TestFunction test([](HIRBuilder& b) {
    Value* sbits = b.Truncate(LoadGPR(b, 1), INT32_TYPE);
    StoreFPR(b, 3, ppc::UnpackSingleKeepNaN(b, sbits));
    b.Return();
  });

  // {single input bits, expected double bits}
  const std::vector<std::pair<uint32_t, uint64_t>> cases = {
      {0x3F800000u, 0x3FF0000000000000ull},  // 1.0
      {0xC0000000u, 0xC000000000000000ull},  // -2.0
      {0x7F800000u, 0x7FF0000000000000ull},  // +inf
      {0xFF800000u, 0xFFF0000000000000ull},  // -inf
      {0x7FC00000u, 0x7FF8000000000000ull},  // qnan stays quiet
      {0x7FA00000u, 0x7FF4000000000000ull},  // snan stays signaling
      {0xFFA00000u, 0xFFF4000000000000ull},  // -snan stays signaling
  };
  for (const auto& c : cases) {
    INFO("single 0x" << std::hex << c.first);
    test.Run([&](PPCContext* ctx) { ctx->r[1] = c.first; },
             [&](PPCContext* ctx) { REQUIRE(GetF64Bits(ctx, 3) == c.second); });
  }
}

// frsp is arithmetic, not data movement: PowerPC quiets signaling NaNs. It
// lowers to a plain double->single->double convert, so the NaN-preserving fix
// must stay scoped to lfs/stfs and leave this path quieting.
TEST_CASE("FRSP_QUIETS_SNAN", "[instr]") {
  TestFunction test([](HIRBuilder& b) {
    StoreFPR(b, 3,
             b.Convert(b.Convert(LoadFPR(b, 1), FLOAT32_TYPE), FLOAT64_TYPE));
    b.Return();
  });
  test.Run([](PPCContext* ctx) { SetF64Bits(ctx, 1, 0x7FF4000000000000ull); },
           [](PPCContext* ctx) {
             uint64_t out = GetF64Bits(ctx, 3);
             INFO("frsp(snan) -> 0x" << std::hex << out);
             REQUIRE(IsDoubleNaN(out));
             REQUIRE((out & (1ull << 51)) != 0);  // quieted
           });
}
