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

// Tessellation control shader for discrete triangle tessellation with 3 control
// points (control point indexed).

layout(vertices = 3) out;

#include "xenos_draw.glsli"

// Input from vertex shader - vertex index as float.
layout(location = 0) in float in_index[];

// Output to tessellation evaluation shader - vertex index as float.
layout(location = 0) out float out_index[];

void main() {
  // Pass through the vertex index.
  out_index[gl_InvocationID] = in_index[gl_InvocationID];

  // Set tessellation levels (only needs to be done by one invocation).
  if (gl_InvocationID == 0) {
    // Xenos creates a uniform grid for triangles, but this can't be reproduced
    // using the tessellator on the PC, so just use what has the closest level
    // of detail.
    // https://www.slideshare.net/blackdevilvikas/next-generation-graphics-programming-on-xbox-360
    //
    // 1.0 already added to the factor on the CPU, according to the images in
    // the slides above.
    float factor = xe_tessellation_factor_range.y;

    gl_TessLevelOuter[0] = factor;
    gl_TessLevelOuter[1] = factor;
    gl_TessLevelOuter[2] = factor;
    gl_TessLevelInner[0] = factor;
  }
}
