/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2025 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

// XE_DXIL_ONLY
// Mip generation compute shader for scaled resolve textures.
// Generates a single mip level by downsampling from the previous level using
// bilinear filtering. Supports texture arrays via SV_DispatchThreadID.z.

cbuffer XeMipGenerateConstants : register(b0) {
  uint2 xe_mip_gen_source_size;   // Source mip dimensions
  uint2 xe_mip_gen_dest_size;     // Destination mip dimensions
};

Texture2DArray<float4> xe_mip_gen_source : register(t0);
RWTexture2DArray<float4> xe_mip_gen_dest : register(u0);
SamplerState xe_sampler_linear_clamp : register(s0);

[numthreads(8, 8, 1)]
void main(uint3 xe_thread_id : SV_DispatchThreadID) {
  // Early out if outside destination bounds.
  [branch] if (any(xe_thread_id.xy >= xe_mip_gen_dest_size)) {
    return;
  }

  // Calculate UV coordinates for sampling the source texture.
  // Add 0.5 to sample at pixel center, then normalize to [0, 1] range.
  // xe_thread_id.z is the array slice index.
  float3 uvw = float3((float2(xe_thread_id.xy) + 0.5f) /
                          float2(xe_mip_gen_dest_size),
                      xe_thread_id.z);

  // Sample with bilinear filtering and write to destination.
  xe_mip_gen_dest[xe_thread_id] =
      xe_mip_gen_source.SampleLevel(xe_sampler_linear_clamp, uvw, 0);
}
