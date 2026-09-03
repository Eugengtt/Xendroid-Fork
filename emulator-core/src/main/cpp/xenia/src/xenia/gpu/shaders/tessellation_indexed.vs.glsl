/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2025. All rights reserved.                                       *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#version 460
#extension GL_GOOGLE_include_directive : require

// Vertex shader for indexed tessellation (discrete/continuous modes).
// Passes the vertex index to the tessellation control shader.

#include "xenos_draw.glsli"

// Output to tessellation control shader - vertex index as float.
layout(location = 0) out float out_index;

// Endian swap for 32-bit value.
uint XeEndianSwap32(uint value, uint endian) {
  if (endian == 0u) {
    // No swap.
    return value;
  } else if (endian == 1u) {
    // 8-in-16.
    return ((value & 0x00FF00FFu) << 8u) | ((value & 0xFF00FF00u) >> 8u);
  } else if (endian == 2u) {
    // 8-in-32.
    return ((value & 0x000000FFu) << 24u) | ((value & 0x0000FF00u) << 8u) |
           ((value & 0x00FF0000u) >> 8u) | ((value & 0xFF000000u) >> 24u);
  } else {
    // 16-in-32.
    return ((value & 0x0000FFFFu) << 16u) | ((value & 0xFFFF0000u) >> 16u);
  }
}

void main() {
  // Only the lower 24 bits of the vertex index are used (tested on an Adreno
  // 200 phone). `((index & 0xFFFFFF) + offset) & 0xFFFFFF` is the same as
  // `(index + offset) & 0xFFFFFF`.
  uint vertex_index = XeEndianSwap32(uint(gl_VertexIndex), xe_vertex_index_endian);
  out_index = float(clamp((vertex_index + xe_vertex_index_offset) & 0xFFFFFFu,
                          xe_vertex_index_min_max.x, xe_vertex_index_min_max.y));
}
