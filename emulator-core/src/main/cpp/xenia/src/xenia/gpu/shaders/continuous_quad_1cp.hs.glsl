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

// Tessellation control shader for continuous quad tessellation with 1 control
// point (patch indexed).
// Uses fractional_even spacing for smooth tessellation.

layout(vertices = 1) out;

#include "xenos_draw.glsli"

// Input from vertex shader - vertex index as float.
layout(location = 0) in float in_index[];

// Output to tessellation evaluation shader - vertex index as float.
layout(location = 0) out float out_index[];

void main() {
  // Pass through the vertex index.
  out_index[gl_InvocationID] = in_index[gl_InvocationID];

  // Set tessellation levels (only one invocation for 1cp).
  // 1.0 already added to the factor on the CPU, according to the images in
  // https://www.slideshare.net/blackdevilvikas/next-generation-graphics-programming-on-xbox-360
  // (fractional_even also requires a factor of at least 2.0).
  float factor = xe_tessellation_factor_range.y;

  gl_TessLevelOuter[0] = factor;
  gl_TessLevelOuter[1] = factor;
  gl_TessLevelOuter[2] = factor;
  gl_TessLevelOuter[3] = factor;
  gl_TessLevelInner[0] = factor;
  gl_TessLevelInner[1] = factor;
}
