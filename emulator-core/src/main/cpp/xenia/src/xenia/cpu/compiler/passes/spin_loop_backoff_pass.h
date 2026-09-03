/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_CPU_COMPILER_PASSES_SPIN_LOOP_BACKOFF_PASS_H_
#define XENIA_CPU_COMPILER_PASSES_SPIN_LOOP_BACKOFF_PASS_H_

#include "xenia/cpu/compiler/compiler_pass.h"

namespace xe {
namespace cpu {
namespace compiler {
namespace passes {

// Collapses the XDK spin-backoff idiom into a single bounded host wait.
//
// Virtually every Xbox 360 title spin-waits with this guest shape:
//   li      r11, 4          ; small constant trip count
//   mtctr   r11
// loop:
//   or      r31,r31,r31     ; xN - SMT priority-hint "nops" (db16cyc)
//   bdnz    loop            ; CTR--, branch if CTR != 0
//
// The per-iteration CTR round trip through guest context (load_context /
// sub / store_context / truncate / compare_ne / branch_true) is among the
// hottest code in spin-heavy titles. When the trip count is a proven small
// compile-time constant and the loop body has no other effects, the whole
// loop is replaced with StoreContext(CTR, 0) followed by OPCODE_SPIN_BACKOFF,
// a real (bounded) host-side wait with no guest-visible effects.
//
// Detection is deliberately conservative; any doubt leaves the loop alone.
// Must run after constant propagation (the predecessor's CTR store needs a
// literal constant) and while the ControlFlowAnalysisPass edges are valid.
class SpinLoopBackoffPass : public CompilerPass {
 public:
  SpinLoopBackoffPass();
  ~SpinLoopBackoffPass() override;

  bool Run(hir::HIRBuilder* builder) override;

 private:
  bool TryCollapseLoop(hir::HIRBuilder* builder, hir::Block* block);
  static bool FindConstantCtrStore(hir::Block* pred, hir::Block* loop_block,
                                   uint64_t* out_trip_count);
};

}  // namespace passes
}  // namespace compiler
}  // namespace cpu
}  // namespace xe

#endif  // XENIA_CPU_COMPILER_PASSES_SPIN_LOOP_BACKOFF_PASS_H_
