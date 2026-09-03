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

// Tessellation control shader for adaptive triangle tessellation.
// Edge factors come from the vertex shader (read from the index buffer).
// Uses fractional_even spacing for smooth tessellation.

layout(vertices = 1) out;

#include "xenos_draw.glsli"

// Input from vertex shader - edge tessellation factor (already processed:
// 1.0 added and clamped to range in the vertex shader).
layout(location = 0) in float in_edge_factor[];

// Output to tessellation evaluation shader - patch index as float.
layout(location = 0) out float out_index[];

void main() {
  // Factors for adaptive tessellation are taken from the index buffer.
  //
  // 1.0 added to the factors according to the images in
  // https://www.slideshare.net/blackdevilvikas/next-generation-graphics-programming-on-xbox-360
  // (fractional_even also requires a factor of at least 2.0), to the min/max it
  // has already been added on the CPU.

  // It appears that on the Xbox 360:
  // - [0] is the factor for the v0->v1 edge.
  // - [1] is the factor for the v1->v2 edge.
  // - [2] is the factor for the v2->v0 edge.
  // Where v0 is the U1V0W0 vertex, v1 is the U0V1W0 vertex, and v2 is the
  // U0V0W1 vertex.
  // The hint at the order was provided in the Code Listing 15 of:
  // http://www.uraldev.ru/files/download/21/Real-Time_Tessellation_on_GPU.pdf
  //
  // In Vulkan/OpenGL:
  // - gl_TessLevelOuter[0] is the factor for the U0 edge (v1->v2).
  // - gl_TessLevelOuter[1] is the factor for the V0 edge (v2->v0).
  // - gl_TessLevelOuter[2] is the factor for the W0 edge (v0->v1).
  //
  // In Xenia's domain shaders, the barycentric coordinates are handled as:
  // 1) gl_TessCoord.xyz -> r0.zyx by Xenia.
  // 2) r0.zyx -> r0.zyx by the guest (because r1.y is set to 0 by Xenia, which
  //    apparently means identity swizzle to games).
  // 3) r0.z * v0 + r0.y * v1 + r0.x * v2 by the guest.
  // With this order, there are no cracks in 4D5307E6 water.
  gl_TessLevelOuter[0] = in_edge_factor[1];  // v1->v2 edge
  gl_TessLevelOuter[1] = in_edge_factor[2];  // v2->v0 edge
  gl_TessLevelOuter[2] = in_edge_factor[0];  // v0->v1 edge

  // Inside factor is the minimum of edge factors.
  gl_TessLevelInner[0] = min(min(gl_TessLevelOuter[0], gl_TessLevelOuter[1]),
                             gl_TessLevelOuter[2]);

  // Output the patch index.
  // Only the lower 24 bits of the vertex index are used (tested on an Adreno
  // 200 phone). `((index & 0xFFFFFF) + offset) & 0xFFFFFF` is the same as
  // `(index + offset) & 0xFFFFFF`.
  out_index[gl_InvocationID] =
      float(clamp((gl_PrimitiveID + xe_vertex_index_offset) & 0xFFFFFFu,
                  xe_vertex_index_min_max.x, xe_vertex_index_min_max.y));
}
