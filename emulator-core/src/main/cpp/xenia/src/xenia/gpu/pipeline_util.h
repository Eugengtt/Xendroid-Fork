/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_GPU_PIPELINE_UTIL_H_
#define XENIA_GPU_PIPELINE_UTIL_H_

#include <cstdint>

namespace xe {
namespace gpu {
namespace pipeline_util {

// Priority levels for async pipeline compilation.
// Higher values = compiled sooner.
constexpr uint8_t kPriorityLowest = 0;     // Writes to unbound RTs only
constexpr uint8_t kPriorityDepthOnly = 1;  // Depth-only writes
constexpr uint8_t kPriorityVisibleRT = 2;  // Writes to any visible RT
constexpr uint8_t kPriorityRT0 = 3;        // Writes to RT0 (main color buffer)

// Converts normalized_color_mask to a 4-bit bitmask of bound render targets.
// normalized_color_mask uses 4 bits per RT (for RGBA components).
inline uint32_t GetBoundRTMaskFromNormalizedColorMask(
    uint32_t normalized_color_mask) {
  return (((normalized_color_mask >> 0) & 0xF) ? 1 : 0) |
         (((normalized_color_mask >> 4) & 0xF) ? 2 : 0) |
         (((normalized_color_mask >> 8) & 0xF) ? 4 : 0) |
         (((normalized_color_mask >> 12) & 0xF) ? 8 : 0);
}

// Calculates pipeline compilation priority based on shader output.
// bound_rts: 4-bit mask of render targets that are bound
// shader_writes_color_targets: 4-bit mask of RTs the shader writes to
// shader_writes_depth: whether the shader writes depth
inline uint8_t CalculatePipelinePriority(uint32_t bound_rts,
                                         uint32_t shader_writes_color_targets,
                                         bool shader_writes_depth) {
  uint32_t visible_writes = bound_rts & shader_writes_color_targets;
  if (visible_writes) {
    // Writes to at least one visible RT - high priority.
    // Extra priority if writing to RT0 (usually main color buffer).
    return (visible_writes & 1) ? kPriorityRT0 : kPriorityVisibleRT;
  }
  if (shader_writes_depth) {
    // Depth-only - medium priority.
    return kPriorityDepthOnly;
  }
  // Writes to unbound RTs only - lowest priority.
  return kPriorityLowest;
}

}  // namespace pipeline_util
}  // namespace gpu
}  // namespace xe

#endif  // XENIA_GPU_PIPELINE_UTIL_H_
