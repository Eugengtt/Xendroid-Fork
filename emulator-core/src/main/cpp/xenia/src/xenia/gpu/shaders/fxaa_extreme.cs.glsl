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

// FXAA compute shader - extreme quality preset (39).

layout(local_size_x = 16, local_size_y = 8, local_size_z = 1) in;

layout(push_constant) uniform FxaaConstants {
  uvec2 xe_fxaa_size;
  vec2 xe_fxaa_size_inv;
};

layout(set = 0, binding = 0, rgb10_a2) uniform writeonly image2D xe_fxaa_dest;
layout(set = 1, binding = 0) uniform sampler2D xe_fxaa_source;

#define FXAA_PC 1
#define FXAA_GLSL_130 1
#define FXAA_QUALITY__PRESET 39

#include "../../../../third_party/fxaa/FXAA3_11.h"

void main() {
  uvec2 pixel_index = gl_GlobalInvocationID.xy;
  if (any(greaterThanEqual(pixel_index, xe_fxaa_size))) {
    return;
  }
  vec2 pos = (vec2(pixel_index) + 0.5) * xe_fxaa_size_inv;
  // FxaaPixelShader arguments - see FXAA3_11.h for documentation.
  // Extreme quality uses higher subpix and lower thresholds.
  vec3 result = FxaaPixelShader(
      pos,
      vec4(0.0),  // fxaaConsolePosPos - unused for PC
      xe_fxaa_source,
      xe_fxaa_source,  // fxaaConsole360TexExpBiasNegOne - unused
      xe_fxaa_source,  // fxaaConsole360TexExpBiasNegTwo - unused
      xe_fxaa_size_inv,
      vec4(0.0),  // fxaaConsoleRcpFrameOpt - unused
      vec4(0.0),  // fxaaConsoleRcpFrameOpt2 - unused
      vec4(0.0),  // fxaaConsole360RcpFrameOpt2 - unused
      1.0,        // fxaaQualitySubpix (higher for extreme)
      0.063,      // fxaaQualityEdgeThreshold (lower for extreme)
      0.0312,     // fxaaQualityEdgeThresholdMin (lower for extreme)
      8.0,        // fxaaConsoleEdgeSharpness - unused
      0.125,      // fxaaConsoleEdgeThreshold - unused
      0.05,       // fxaaConsoleEdgeThresholdMin - unused
      vec4(0.0)   // fxaaConsole360ConstDir - unused
  ).rgb;
  imageStore(xe_fxaa_dest, ivec2(pixel_index), vec4(result, 1.0));
}
