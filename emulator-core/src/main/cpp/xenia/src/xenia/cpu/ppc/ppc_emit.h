/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_CPU_PPC_PPC_EMIT_H_
#define XENIA_CPU_PPC_PPC_EMIT_H_

namespace xe {
namespace cpu {
namespace hir {
class HIRBuilder;
class Value;
}  // namespace hir
namespace ppc {

void RegisterEmitCategoryAltivec();
void RegisterEmitCategoryALU();
void RegisterEmitCategoryControl();
void RegisterEmitCategoryFPU();
void RegisterEmitCategoryMemory();

// Single<->double conversions that keep signaling NaNs intact, matching
// PowerPC lfs/stfs. Exposed for testing; see ppc_emit_memory.cc.
hir::Value* PackSingleKeepNaN(hir::HIRBuilder& f, hir::Value* value);
hir::Value* UnpackSingleKeepNaN(hir::HIRBuilder& f, hir::Value* value);

}  // namespace ppc
}  // namespace cpu
}  // namespace xe

#endif  // XENIA_CPU_PPC_PPC_EMIT_H_
