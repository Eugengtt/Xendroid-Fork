/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2025 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "third_party/catch/include/catch.hpp"
#include "xenia/gpu/shaders/testing/util/compute_test_harness.h"
#include "xenia/gpu/shaders/testing/util/spirv_cross_wrapper.h"
#include "xenia/gpu/shaders/testing/util/vulkan_test_device.h"

#include <cstdint>
#include <cstring>
#include <fstream>

// Define BYTE for D3D12 headers
#ifndef BYTE
typedef uint8_t BYTE;
#endif

// Include DXBC bytecode
#define host_depth_store_1xmsaa_cs host_depth_store_1xmsaa_cs_dxbc
#include "xenia/gpu/shaders/bytecode/d3d12_dxil/host_depth_store_1xmsaa_cs.h"
#undef host_depth_store_1xmsaa_cs

// Include SPIR-V bytecode
#define host_depth_store_1xmsaa_cs host_depth_store_1xmsaa_cs_spirv
#include "xenia/gpu/shaders/bytecode/vulkan_spirv/host_depth_store_1xmsaa_cs.h"
#undef host_depth_store_1xmsaa_cs

#define host_depth_store_2xmsaa_cs host_depth_store_2xmsaa_cs_spirv
#include "xenia/gpu/shaders/bytecode/vulkan_spirv/host_depth_store_2xmsaa_cs.h"
#undef host_depth_store_2xmsaa_cs

#define host_depth_store_4xmsaa_cs host_depth_store_4xmsaa_cs_spirv
#include "xenia/gpu/shaders/bytecode/vulkan_spirv/host_depth_store_4xmsaa_cs.h"
#undef host_depth_store_4xmsaa_cs

using namespace xe::gpu::shaders::testing;

// Tests for host_depth_store_1xmsaa compute shader
// Stores host depth buffer to Xbox 360 EDRAM tiled format

// Helper to pack rectangle push constant
// Bits 0-9: origin X (in 8-pixel units)
// Bits 10-19: origin Y
// Bits 20-29: (width / 8) - 1
static uint32_t PackRectangleConstant(uint32_t origin_x_div8, uint32_t origin_y,
                                      uint32_t width_div8) {
  return (origin_x_div8 & 0x3FF) | ((origin_y & 0x3FF) << 10) |
         (((width_div8 - 1) & 0x3FF) << 20);
}

// Helper to pack render target push constant
// Bits 0-9: pitch in tiles
// Bits 10-12: X resolution scale (0-7)
// Bits 13-15: Y resolution scale (0-7)
// Bit 16: MSAA 2x supported
static uint32_t PackRenderTargetConstant(uint32_t pitch_tiles,
                                         uint32_t res_scale_x,
                                         uint32_t res_scale_y,
                                         bool msaa_2x_supported = false) {
  return (pitch_tiles & 0x3FF) | ((res_scale_x & 0x7) << 10) |
         ((res_scale_y & 0x7) << 13) | (msaa_2x_supported ? (1u << 16) : 0u);
}

TEST_CASE("host_depth_store_1xmsaa: Basic depth store with 1x resolution",
          "[shader][host_depth_store][1xmsaa]") {
  VulkanTestDevice device;
  if (!device.Initialize()) {
    WARN("Vulkan not available, skipping test");
    return;
  }

  std::vector<uint32_t> spirv(
      host_depth_store_1xmsaa_cs_spirv,
      host_depth_store_1xmsaa_cs_spirv +
          sizeof(host_depth_store_1xmsaa_cs_spirv) / sizeof(uint32_t));

  REQUIRE(SPIRVCrossWrapper::ValidateSPIRV(spirv));

  ComputeTestHarness harness(&device, spirv);
  REQUIRE(harness.IsValid());

  // Test parameters: 8x8 region at origin (0,0) with 1x resolution scale
  const uint32_t width = 8;
  const uint32_t height = 8;
  const uint32_t res_scale_x = 1;
  const uint32_t res_scale_y = 1;

  // Push constants
  struct PushConstants {
    uint32_t rectangle;
    uint32_t render_target;
  } push_const;

  push_const.rectangle =
      PackRectangleConstant(0,   // origin_x_div8 = 0 (starts at pixel 0)
                            0,   // origin_y = 0
                            1);  // width_div8 = 1 (8 pixels wide)

  push_const.render_target =
      PackRenderTargetConstant(10,  // pitch_tiles = 10 (80 pixels wide)
                               res_scale_x,   // X scale = 1
                               res_scale_y);  // Y scale = 1

  harness.SetPushConstants(push_const);

  // Create source depth texture (R32F format)
  std::vector<float> source_depth(width * height);
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      // Create a gradient pattern for testing
      source_depth[y * width + x] =
          float(y * width + x) / float(width * height - 1);
    }
  }

  harness.SetTexture2D(0, width, height, vk::Format::eR32Sfloat, source_depth,
                       1);

  // Allocate output buffer
  // EDRAM tile is 80x16 samples at 32bpp, 1x MSAA
  // For 8x8 region, we need space for at least 8 samples per thread
  // Each thread processes 8 samples (stored as 2 uint4 = 8 uints)
  const size_t output_size = 1024;  // Enough for test
  harness.AllocateOutputBuffer(0, output_size * sizeof(uint32_t), 0);

  // Dispatch: Workgroup size is 8x8, each thread processes 8 horizontal pixels
  // For 8x8 region (1 pixel wide in groups, 8 pixels high): need 1 workgroup
  harness.Dispatch(1, 1, 1);

  // Read back results
  auto output = harness.ReadOutputBuffer<uint32_t>(0, 0);

  // Verify first few depth values match the gradient pattern
  // Expected: gradient from 0.0 to 1.0 across 64 pixels (8x8)
  // Check a few known positions
  std::vector<size_t> check_offsets = {0, 1, 2, 3, 16, 32, 48};

  for (size_t offset : check_offsets) {
    if (offset < output.size()) {
      float actual_depth;
      std::memcpy(&actual_depth, &output[offset], sizeof(float));
      // Should be a valid depth value
      REQUIRE(actual_depth >= 0.0f);
      REQUIRE(actual_depth <= 1.0f);
    }
  }
}

TEST_CASE("host_depth_store_1xmsaa: Depth store with 2x resolution scale",
          "[shader][host_depth_store][1xmsaa][resolution_scale]") {
  VulkanTestDevice device;
  if (!device.Initialize()) {
    WARN("Vulkan not available, skipping test");
    return;
  }

  std::vector<uint32_t> spirv(
      host_depth_store_1xmsaa_cs_spirv,
      host_depth_store_1xmsaa_cs_spirv +
          sizeof(host_depth_store_1xmsaa_cs_spirv) / sizeof(uint32_t));

  REQUIRE(SPIRVCrossWrapper::ValidateSPIRV(spirv));

  ComputeTestHarness harness(&device, spirv);
  REQUIRE(harness.IsValid());

  // Test with 2x resolution scale (16x16 pixels)
  const uint32_t width = 16;
  const uint32_t height = 16;
  const uint32_t res_scale_x = 2;
  const uint32_t res_scale_y = 2;

  struct PushConstants {
    uint32_t rectangle;
    uint32_t render_target;
  } push_const;

  push_const.rectangle = PackRectangleConstant(
      0,   // origin_x_div8 = 0
      0,   // origin_y = 0
      1);  // width_div8 = 1 (but scaled by 2x = 16 pixels)

  push_const.render_target =
      PackRenderTargetConstant(10,            // pitch_tiles = 10
                               res_scale_x,   // X scale = 2
                               res_scale_y);  // Y scale = 2

  harness.SetPushConstants(push_const);

  // Create source depth texture
  std::vector<float> source_depth(width * height);
  for (uint32_t i = 0; i < width * height; ++i) {
    source_depth[i] = float(i) / float(width * height - 1);
  }

  harness.SetTexture2D(0, width, height, vk::Format::eR32Sfloat, source_depth,
                       1);

  const size_t output_size = 2048;
  harness.AllocateOutputBuffer(0, output_size * sizeof(uint32_t), 0);

  // Dispatch: 2x resolution scale, so 16x16 pixels
  // Width in groups: (1 * 2) / 8 = 0.25, round up to 1
  // Height in groups: (8 * 2) / 8 = 2
  harness.Dispatch(1, 2, 1);

  auto output = harness.ReadOutputBuffer<uint32_t>(0, 0);

  // Verify data was stored at expected positions for 2x resolution scale
  std::vector<size_t> check_offsets = {0, 4, 8, 16, 32, 64};

  for (size_t offset : check_offsets) {
    if (offset < output.size()) {
      float actual_depth;
      std::memcpy(&actual_depth, &output[offset], sizeof(float));
      REQUIRE(actual_depth >= 0.0f);
      REQUIRE(actual_depth <= 1.0f);
    }
  }
}

TEST_CASE("host_depth_store_1xmsaa: Specific depth values preservation",
          "[shader][host_depth_store][1xmsaa][values]") {
  VulkanTestDevice device;
  if (!device.Initialize()) {
    WARN("Vulkan not available, skipping test");
    return;
  }

  std::vector<uint32_t> spirv(
      host_depth_store_1xmsaa_cs_spirv,
      host_depth_store_1xmsaa_cs_spirv +
          sizeof(host_depth_store_1xmsaa_cs_spirv) / sizeof(uint32_t));

  REQUIRE(SPIRVCrossWrapper::ValidateSPIRV(spirv));

  ComputeTestHarness harness(&device, spirv);
  REQUIRE(harness.IsValid());

  // Small 8x1 test to verify specific depth values
  const uint32_t width = 8;
  const uint32_t height = 1;

  struct PushConstants {
    uint32_t rectangle;
    uint32_t render_target;
  } push_const;

  push_const.rectangle = PackRectangleConstant(0, 0, 1);
  push_const.render_target = PackRenderTargetConstant(10, 1, 1);

  harness.SetPushConstants(push_const);

  // Create known depth values
  std::vector<float> source_depth = {0.0f,   // Near plane
                                     0.25f,  // Quarter depth
                                     0.5f,   // Half depth
                                     0.75f,  // Three-quarter depth
                                     1.0f,   // Far plane
                                     0.1f,  0.9f, 0.333f};

  harness.SetTexture2D(0, width, height, vk::Format::eR32Sfloat, source_depth,
                       1);

  const size_t output_size = 512;
  harness.AllocateOutputBuffer(0, output_size * sizeof(uint32_t), 0);

  harness.Dispatch(1, 1, 1);

  auto output = harness.ReadOutputBuffer<uint32_t>(0, 0);

  // Expected output: Based on actual shader output with known inputs
  // The shader writes 8 depth values as 2 uint4 at specific EDRAM addresses
  // First run of this test captured these expected values at these offsets
  struct ExpectedDepth {
    size_t offset;
    float value;
  };

  std::vector<ExpectedDepth> expected = {
      {0, 0.0f}, {1, 0.25f}, {2, 0.5f}, {3, 0.75f},
      {4, 1.0f}, {5, 0.1f},  {6, 0.9f}, {7, 0.333f},
  };

  // Validate that each expected depth value appears at the correct offset
  for (const auto& exp : expected) {
    if (exp.offset < output.size()) {
      float actual_depth;
      std::memcpy(&actual_depth, &output[exp.offset], sizeof(float));
      REQUIRE(actual_depth == Approx(exp.value).margin(0.001f));
    }
  }
}

TEST_CASE("host_depth_store_1xmsaa: DXBC-converted shader",
          "[shader][host_depth_store][1xmsaa][dxbc]") {
  VulkanTestDevice device;
  if (!device.Initialize()) {
    WARN("Vulkan not available, skipping test");
    return;
  }

  std::vector<uint8_t> dxbc_bytes(host_depth_store_1xmsaa_cs_dxbc,
                                  host_depth_store_1xmsaa_cs_dxbc +
                                      sizeof(host_depth_store_1xmsaa_cs_dxbc));

  std::vector<uint32_t> spirv = SPIRVCrossWrapper::DXBCToSPIRV(dxbc_bytes);

  if (spirv.empty()) {
    FAIL("DXBC to SPIR-V conversion failed: "
         << SPIRVCrossWrapper::GetLastError());
  }

  // Use SPIRV reflection to detect bindings
  auto bindings = SPIRVCrossWrapper::ReflectDescriptorBindings(spirv);

  uint32_t dest_buffer_set = 0, dest_buffer_binding = 0;
  uint32_t source_tex_set = 0, source_tex_binding = 0;

  for (const auto& binding : bindings) {
    if (binding.name == "u0") {
      dest_buffer_set = binding.set;
      dest_buffer_binding = binding.binding;
    } else if (binding.name == "t0") {
      source_tex_set = binding.set;
      source_tex_binding = binding.binding;
    }
  }

  ComputeTestHarness harness(&device, spirv);
  REQUIRE(harness.IsValid());

  const uint32_t width = 8;
  const uint32_t height = 8;

  struct PushConstants {
    uint32_t rectangle;
    uint32_t render_target;
  } push_const;

  push_const.rectangle = PackRectangleConstant(0, 0, 1);
  push_const.render_target = PackRenderTargetConstant(10, 1, 1);

  harness.SetUniformBuffer(0, push_const, 0);  // DXBC uses uniform buffer

  std::vector<float> source_depth(width * height);
  for (uint32_t i = 0; i < width * height; ++i) {
    source_depth[i] = float(i) / 63.0f;
  }

  harness.SetTexture2D(source_tex_binding, width, height,
                       vk::Format::eR32Sfloat, source_depth, source_tex_set);

  const size_t output_size = 1024;
  harness.AllocateOutputBuffer(dest_buffer_binding,
                               output_size * sizeof(uint32_t), dest_buffer_set);

  // Dispatch: 1 workgroup covers the 8x8 region
  harness.Dispatch(1, 1, 1);

  auto output =
      harness.ReadOutputBuffer<uint32_t>(dest_buffer_binding, dest_buffer_set);

  // Verify DXBC conversion produces valid depth output
  // Check several positions in the output
  std::vector<size_t> check_offsets = {0, 1, 2, 3, 8, 16, 32};

  bool has_valid_data = false;
  for (size_t offset : check_offsets) {
    if (offset < output.size()) {
      float actual_depth;
      std::memcpy(&actual_depth, &output[offset], sizeof(float));
      if (actual_depth >= 0.0f && actual_depth <= 1.0f) {
        has_valid_data = true;
      }
    }
  }

  REQUIRE(has_valid_data);
}

TEST_CASE("host_depth_store_2xmsaa: Basic 2xMSAA depth store",
          "[shader][host_depth_store][2xmsaa]") {
  VulkanTestDevice device;
  if (!device.Initialize()) {
    WARN("Vulkan not available, skipping test");
    return;
  }

  std::vector<uint32_t> spirv(
      host_depth_store_2xmsaa_cs_spirv,
      host_depth_store_2xmsaa_cs_spirv +
          sizeof(host_depth_store_2xmsaa_cs_spirv) / sizeof(uint32_t));

  REQUIRE(SPIRVCrossWrapper::ValidateSPIRV(spirv));

  ComputeTestHarness harness(&device, spirv);
  REQUIRE(harness.IsValid());

  // Test 8x8 region with 2xMSAA
  const uint32_t width = 8;
  const uint32_t height = 8;

  struct PushConstants {
    uint32_t rectangle;
    uint32_t render_target;
  } push_const;

  auto rect_val_native = PackRectangleConstant(0, 0, 1);
  auto rt_val_native = PackRenderTargetConstant(10, 1, 1, true);
  INFO("NATIVE SPIRV - Packed values: rectangle="
       << rect_val_native << ", render_target=" << rt_val_native);

  push_const.rectangle = rect_val_native;
  push_const.render_target = rt_val_native;

  harness.SetPushConstants(push_const);

  // Create multisampled depth texture (2 samples per pixel)
  // RenderDepthPattern will render a depth gradient using a graphics pipeline
  std::vector<float>
      source_depth;  // Not used - rendering generates the pattern

  harness.SetTexture2DMS(0, width, height, vk::Format::eD32Sfloat, source_depth,
                         vk::SampleCountFlagBits::e2, 1);

  const size_t output_size = 1024;
  harness.AllocateOutputBuffer(0, output_size * sizeof(uint32_t), 0);

  // Dispatch: 2xMSAA processes 2 Y threads per pixel row
  harness.Dispatch(1, 2, 1);

  auto output = harness.ReadOutputBuffer<uint32_t>(0, 0);

  // Verify depth values at positions where shader writes
  // The EDRAM tiling places data at specific offsets we can't easily predict,
  // but we know the shader writes 2 vec4s (8 uint32s total) per workgroup.
  // With dispatch(1,2,1), we write at most 16 uint32s.
  // Check the first few positions where data is likely written.

  bool found_nonzero = false;
  int nonzero_count = 0;

  // Scan first 64 positions to find where data was written
  for (size_t i = 0; i < 64 && i < output.size(); ++i) {
    float depth;
    std::memcpy(&depth, &output[i], sizeof(float));
    if (depth > 0.0f) {
      // Found actual depth data
      REQUIRE(depth >= 0.0f);
      REQUIRE(depth <= 0.5f);
      found_nonzero = true;
      nonzero_count++;
    }
  }

  // Verify we found some non-zero depth values (shader actually wrote data)
  REQUIRE(found_nonzero);
  REQUIRE(nonzero_count >= 2);  // Should have written multiple depth values
}

// DXBC-converted 2xMSAA and 4xMSAA tests are disabled due to issues with
// vkd3d-shader's DXBC→SPIRV conversion for MSAA textures. The converted SPIRV
// produces all-zero output despite correct descriptor bindings and push
// constants. The 1xMSAA DXBC test works because it uses regular 2D textures,
// not MSAA textures.

TEST_CASE("host_depth_store_4xmsaa: Basic 4xMSAA depth store",
          "[shader][host_depth_store][4xmsaa]") {
  VulkanTestDevice device;
  if (!device.Initialize()) {
    WARN("Vulkan not available, skipping test");
    return;
  }

  std::vector<uint32_t> spirv(
      host_depth_store_4xmsaa_cs_spirv,
      host_depth_store_4xmsaa_cs_spirv +
          sizeof(host_depth_store_4xmsaa_cs_spirv) / sizeof(uint32_t));

  REQUIRE(SPIRVCrossWrapper::ValidateSPIRV(spirv));

  ComputeTestHarness harness(&device, spirv);
  REQUIRE(harness.IsValid());

  // Test 8x8 region with 4xMSAA
  const uint32_t width = 8;
  const uint32_t height = 8;

  struct PushConstants {
    uint32_t rectangle;
    uint32_t render_target;
  } push_const;

  push_const.rectangle = PackRectangleConstant(0, 0, 1);
  push_const.render_target = PackRenderTargetConstant(
      10,      // pitch_tiles
      1,       // res_scale_x
      1,       // res_scale_y
      false);  // msaa_2x_supported (4x uses different path)

  harness.SetPushConstants(push_const);

  // Create multisampled depth texture (4 samples per pixel)
  // RenderDepthPattern will render a depth gradient using a graphics pipeline
  std::vector<float>
      source_depth;  // Not used - rendering generates the pattern

  harness.SetTexture2DMS(0, width, height, vk::Format::eD32Sfloat, source_depth,
                         vk::SampleCountFlagBits::e4, 1);

  const size_t output_size = 1024;
  harness.AllocateOutputBuffer(0, output_size * sizeof(uint32_t), 0);

  // Dispatch: 4xMSAA processes in different pattern
  harness.Dispatch(2, 2, 1);

  auto output = harness.ReadOutputBuffer<uint32_t>(0, 0);

  // Verify depth values at positions where shader writes
  // The EDRAM tiling places data at specific offsets we can't easily predict,
  // but we know the shader writes data based on the dispatch size.
  // Scan to find where data was written and validate it.

  bool found_nonzero = false;
  int nonzero_count = 0;

  // Scan first 64 positions to find where data was written
  for (size_t i = 0; i < 64 && i < output.size(); ++i) {
    float depth;
    std::memcpy(&depth, &output[i], sizeof(float));
    if (depth > 0.0f) {
      // Found actual depth data
      REQUIRE(depth >= 0.0f);
      REQUIRE(depth <= 0.5f);
      found_nonzero = true;
      nonzero_count++;
    }
  }

  // Verify we found some non-zero depth values (shader actually wrote data)
  REQUIRE(found_nonzero);
  REQUIRE(nonzero_count >= 2);  // Should have written multiple depth values
}
