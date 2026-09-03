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

// Tessellation control shader for adaptive quad tessellation.
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
  // 1.0 added to the factors according to the images in
  // https://www.slideshare.net/blackdevilvikas/next-generation-graphics-programming-on-xbox-360
  // (fractional_even also requires a factor of at least 2.0), to the min/max it
  // has already been added on the CPU.

  // Vulkan/OpenGL (goes in a direction along the perimeter):
  // [0] - between U0V1 and U0V0.
  // [1] - between U0V0 and U1V0.
  // [2] - between U1V0 and U1V1.
  // [3] - between U1V1 and U0V1.
  //
  // Xbox 360 factors go along the perimeter too according to the example of
  // edge factors in Next Generation Graphics Programming on Xbox 360.
  // However, if v0->v1... that seems to be working for triangle patches applies
  // here too, with the swizzle Xenia uses in domain shaders:
  // [0] - between U0V0 and U1V0.
  // [1] - between U1V0 and U1V1.
  // [2] - between U1V1 and U0V1.
  // [3] - between U0V1 and U0V0.
  gl_TessLevelOuter[0] = in_edge_factor[3];
  gl_TessLevelOuter[1] = in_edge_factor[0];
  gl_TessLevelOuter[2] = in_edge_factor[1];
  gl_TessLevelOuter[3] = in_edge_factor[2];

  // On the Xbox 360, according to the presentation, the inside factor is the
  // minimum of the factors of the edges along the axis.
  // Vulkan/OpenGL:
  // [0] - along U.
  // [1] - along V.
  gl_TessLevelInner[0] = min(gl_TessLevelOuter[1], gl_TessLevelOuter[3]);
  gl_TessLevelInner[1] = min(gl_TessLevelOuter[0], gl_TessLevelOuter[2]);

  // Output the patch index.
  // Only the lower 24 bits of the vertex index are used (tested on an Adreno
  // 200 phone). `((index & 0xFFFFFF) + offset) & 0xFFFFFF` is the same as
  // `(index + offset) & 0xFFFFFF`.
  out_index[gl_InvocationID] =
      float(clamp((gl_PrimitiveID + xe_vertex_index_offset) & 0xFFFFFFu,
                  xe_vertex_index_min_max.x, xe_vertex_index_min_max.y));
}
