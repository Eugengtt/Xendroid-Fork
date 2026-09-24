/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2025 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

// XE_DXIL_ONLY
// Mip generation compute shader for scaled resolve 3D textures.
// Generates a single mip level by downsampling from the previous level using
// trilinear filtering.

cbuffer XeMipGenerateConstants : register(b0) {
  uint3 xe_mip_gen_source_size;   // Source mip dimensions (width, height, depth)
  uint xe_mip_gen_padding0;       // Padding for alignment
  uint3 xe_mip_gen_dest_size;     // Destination mip dimensions
  uint xe_mip_gen_padding1;       // Padding for alignment
};

Texture3D<float4> xe_mip_gen_source : register(t0);
RWTexture3D<float4> xe_mip_gen_dest : register(u0);
SamplerState xe_sampler_linear_clamp : register(s0);

[numthreads(4, 4, 4)]
void main(uint3 xe_thread_id : SV_DispatchThreadID) {
  // Early out if outside destination bounds.
  [branch] if (any(xe_thread_id >= xe_mip_gen_dest_size)) {
    return;
  }

  // Calculate UVW coordinates for sampling the source texture.
  // Add 0.5 to sample at voxel center, then normalize to [0, 1] range.
  float3 uvw = (float3(xe_thread_id) + 0.5f) / float3(xe_mip_gen_dest_size);

  // Sample with trilinear filtering and write to destination.
  xe_mip_gen_dest[xe_thread_id] =
      xe_mip_gen_source.SampleLevel(xe_sampler_linear_clamp, uvw, 0);
}
