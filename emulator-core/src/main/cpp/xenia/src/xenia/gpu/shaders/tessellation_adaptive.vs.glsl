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

// Vertex shader for adaptive tessellation.
// Reads edge tessellation factors from the index buffer (as float32) and passes
// them to the tessellation control shader.

#include "xenos_draw.glsli"

// Output to tessellation control shader - edge tessellation factor.
layout(location = 0) out float out_edge_factor;

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
  // The Xbox 360's GPU accepts the float32 tessellation factors for edges
  // through a special kind of an index buffer.
  // While 4D5307F2 sets the factors to 0 for frustum-culled (quad) patches, in
  // 4D5307E6 only allowing patches with factors above 0 makes distant
  // (triangle) patches disappear - it appears that there are no special values
  // for culled patches on the Xbox 360 (unlike zero, negative and NaN on
  // Direct3D 11).
  //
  // The vertex index (gl_VertexIndex) contains the raw edge factor bits from
  // the index buffer. We need to endian swap and interpret as float.
  uint factor_bits = XeEndianSwap32(uint(gl_VertexIndex), xe_vertex_index_endian);
  float factor = uintBitsToFloat(factor_bits) + 1.0;
  out_edge_factor = clamp(factor, xe_tessellation_factor_range.x,
                          xe_tessellation_factor_range.y);
}
