/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2021 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/gpu/vulkan/vulkan_primitive_processor.h"

#include <cstdint>

#include "xenia/base/assert.h"
#include "xenia/base/cvar.h"
#include "xenia/base/logging.h"
#include "xenia/gpu/vulkan/deferred_command_buffer.h"
#include "xenia/gpu/vulkan/vulkan_command_processor.h"
#include "xenia/ui/vulkan/vulkan_util.h"

DEFINE_bool(
    vulkan_avoid_geometry_shaders, true,
    "Route guest primitives that would otherwise need a geometry shader through "
    "cheaper host paths on tile-based GPUs (Adreno/Turnip), where the "
    "geometry-shader stage serializes the binning pass. Applies to point "
    "sprites (expanded in the vertex shader as a triangle strip) and quad lists "
    "(expanded on the CPU to a triangle list). Rectangle lists always stay on "
    "the geometry shader. Read once at GPU "
    "initialization, so a change takes effect on emulator restart. Disable for "
    "byte-identical pre-optimization (geometry-shader) behavior and A/B "
    "debugging.",
    "Vulkan");

namespace xe {
namespace gpu {
namespace vulkan {

VulkanPrimitiveProcessor::~VulkanPrimitiveProcessor() { Shutdown(true); }

bool VulkanPrimitiveProcessor::Initialize() {
  const ui::vulkan::VulkanDevice* const vulkan_device =
      command_processor_.GetVulkanDevice();
  const ui::vulkan::VulkanDevice::Properties& device_properties =
      vulkan_device->properties();
  // Tier 3 #7: avoid the geometry-shader stage for guest point sprites and quad
  // lists on tile-based GPUs (Adreno/Turnip), where the GS serializes the
  // binning pass. When vulkan_avoid_geometry_shaders is set:
  //   - point sprites take the kPointListAsTriangleStrip VS expansion (pass
  //     false for point_sprites_supported_without_vs_expansion);
  //   - quad lists take CPU index expansion to a triangle LIST (pass false for
  //     quad_lists_supported -> convert_quad_lists_to_triangle_lists_).
  // The CPU quad path splits the q0-q2 diagonal as a triangle list, while the
  // GS splits q1-q3 as a 0,1,3,2 strip; both carry an unresolved "find correct
  // order" TODO (neither is verified against real Xbox 360 hardware). For
  // planar convex quads (UI/sprites, the common case) the two are pixel- and
  // winding-identical (all triangles stay CCW), so culling is unchanged; they
  // diverge only on non-planar/concave quads. The kill-switch restores the GS
  // path if a title regresses.
  // Rectangle lists ALWAYS stay on the GS (their VS path is the unfinished TODO
  // at spirv_shader_translator.cc:1317; the draw gate would reject
  // kRectangleListAsTriangleStrip). The flag is consumed once here
  // (InitializeCommon sizes the builtin index buffer from it), so a change
  // takes effect on emulator restart.
  if (!InitializeCommon(
          // full_32bit_vertex_indices_supported, triangle_fans_supported,
          device_properties.fullDrawIndexUint32, device_properties.triangleFans,
          // line_loops_supported, quad_lists_supported (gated -> CPU tri-list):
          false,
          device_properties.geometryShader &&
              !cvars::vulkan_avoid_geometry_shaders,
          // point_sprites_supported_without_vs_expansion (gated -> VS path):
          device_properties.geometryShader &&
              !cvars::vulkan_avoid_geometry_shaders,
          // rectangle_lists_supported_without_vs_expansion (always GS):
          device_properties.geometryShader)) {
    Shutdown();
    return false;
  }
  frame_index_buffer_pool_ =
      std::make_unique<ui::vulkan::VulkanUploadBufferPool>(
          vulkan_device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
          std::max(size_t(kMinRequiredConvertedIndexBufferSize),
                   ui::GraphicsUploadBufferPool::kDefaultPageSize));
  return true;
}

void VulkanPrimitiveProcessor::Shutdown(bool from_destructor) {
  const ui::vulkan::VulkanDevice* const vulkan_device =
      command_processor_.GetVulkanDevice();
  const ui::vulkan::VulkanDevice::Functions& dfn = vulkan_device->functions();
  const VkDevice device = vulkan_device->device();

  frame_index_buffers_.clear();
  frame_index_buffer_pool_.reset();
  ui::vulkan::util::DestroyAndNullHandle(dfn.vkDestroyBuffer, device,
                                         builtin_index_buffer_upload_);
  ui::vulkan::util::DestroyAndNullHandle(dfn.vkFreeMemory, device,
                                         builtin_index_buffer_upload_memory_);
  ui::vulkan::util::DestroyAndNullHandle(dfn.vkDestroyBuffer, device,
                                         builtin_index_buffer_);
  ui::vulkan::util::DestroyAndNullHandle(dfn.vkFreeMemory, device,
                                         builtin_index_buffer_memory_);

  if (!from_destructor) {
    ShutdownCommon();
  }
}

void VulkanPrimitiveProcessor::CompletedSubmissionUpdated() {
  if (builtin_index_buffer_upload_ != VK_NULL_HANDLE &&
      command_processor_.GetCompletedSubmission() >=
          builtin_index_buffer_upload_submission_) {
    const ui::vulkan::VulkanDevice* const vulkan_device =
        command_processor_.GetVulkanDevice();
    const ui::vulkan::VulkanDevice::Functions& dfn = vulkan_device->functions();
    const VkDevice device = vulkan_device->device();
    ui::vulkan::util::DestroyAndNullHandle(dfn.vkDestroyBuffer, device,
                                           builtin_index_buffer_upload_);
    ui::vulkan::util::DestroyAndNullHandle(dfn.vkFreeMemory, device,
                                           builtin_index_buffer_upload_memory_);
  }
}

void VulkanPrimitiveProcessor::BeginSubmission() {
  if (builtin_index_buffer_upload_ != VK_NULL_HANDLE &&
      builtin_index_buffer_upload_submission_ == UINT64_MAX) {
    // No need to submit deferred barriers - builtin_index_buffer_ has never
    // been used yet, and builtin_index_buffer_upload_ is written before
    // submitting commands reading it.

    command_processor_.EndRenderPass();

    DeferredCommandBuffer& command_buffer =
        command_processor_.deferred_command_buffer();

    command_processor_.InsertDebugMarker(
        "Builtin Index Buffer Upload: %zu bytes", builtin_index_buffer_size_);

    VkBufferCopy* copy_region = command_buffer.CmdCopyBufferEmplace(
        builtin_index_buffer_upload_, builtin_index_buffer_, 1);
    copy_region->srcOffset = 0;
    copy_region->dstOffset = 0;
    copy_region->size = builtin_index_buffer_size_;

    command_processor_.PushBufferMemoryBarrier(
        builtin_index_buffer_, 0, VK_WHOLE_SIZE, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_INDEX_READ_BIT);

    builtin_index_buffer_upload_submission_ =
        command_processor_.GetCurrentSubmission();
  }
}

void VulkanPrimitiveProcessor::BeginFrame() {
  frame_index_buffer_pool_->Reclaim(command_processor_.GetCompletedFrame());
}

void VulkanPrimitiveProcessor::EndSubmission() {
  frame_index_buffer_pool_->FlushWrites();
}

void VulkanPrimitiveProcessor::EndFrame() {
  ClearPerFrameCache();
  frame_index_buffers_.clear();
}

bool VulkanPrimitiveProcessor::InitializeBuiltinIndexBuffer(
    size_t size_bytes, std::function<void(void*)> fill_callback) {
  assert_not_zero(size_bytes);
  assert_true(builtin_index_buffer_ == VK_NULL_HANDLE);
  assert_true(builtin_index_buffer_memory_ == VK_NULL_HANDLE);
  assert_true(builtin_index_buffer_upload_ == VK_NULL_HANDLE);
  assert_true(builtin_index_buffer_upload_memory_ == VK_NULL_HANDLE);

  const ui::vulkan::VulkanDevice* const vulkan_device =
      command_processor_.GetVulkanDevice();
  const ui::vulkan::VulkanDevice::Functions& dfn = vulkan_device->functions();
  const VkDevice device = vulkan_device->device();

  builtin_index_buffer_size_ = VkDeviceSize(size_bytes);
  if (!ui::vulkan::util::CreateDedicatedAllocationBuffer(
          vulkan_device, builtin_index_buffer_size_,
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
          ui::vulkan::util::MemoryPurpose::kDeviceLocal, builtin_index_buffer_,
          builtin_index_buffer_memory_)) {
    XELOGE(
        "Vulkan primitive processor: Failed to create the built-in index "
        "buffer GPU resource with {} bytes",
        size_bytes);
    return false;
  }
  uint32_t upload_memory_type;
  if (!ui::vulkan::util::CreateDedicatedAllocationBuffer(
          vulkan_device, builtin_index_buffer_size_,
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
          ui::vulkan::util::MemoryPurpose::kUpload,
          builtin_index_buffer_upload_, builtin_index_buffer_upload_memory_,
          &upload_memory_type)) {
    XELOGE(
        "Vulkan primitive processor: Failed to create the built-in index "
        "buffer upload resource with {} bytes",
        size_bytes);
    ui::vulkan::util::DestroyAndNullHandle(dfn.vkDestroyBuffer, device,
                                           builtin_index_buffer_);
    ui::vulkan::util::DestroyAndNullHandle(dfn.vkFreeMemory, device,
                                           builtin_index_buffer_memory_);
    return false;
  }

  void* mapping;
  if (dfn.vkMapMemory(device, builtin_index_buffer_upload_memory_, 0,
                      VK_WHOLE_SIZE, 0, &mapping) != VK_SUCCESS) {
    XELOGE(
        "Vulkan primitive processor: Failed to map the built-in index buffer "
        "upload resource with {} bytes",
        size_bytes);
    ui::vulkan::util::DestroyAndNullHandle(dfn.vkDestroyBuffer, device,
                                           builtin_index_buffer_upload_);
    ui::vulkan::util::DestroyAndNullHandle(dfn.vkFreeMemory, device,
                                           builtin_index_buffer_upload_memory_);
    ui::vulkan::util::DestroyAndNullHandle(dfn.vkDestroyBuffer, device,
                                           builtin_index_buffer_);
    ui::vulkan::util::DestroyAndNullHandle(dfn.vkFreeMemory, device,
                                           builtin_index_buffer_memory_);
    return false;
  }
  fill_callback(mapping);
  ui::vulkan::util::FlushMappedMemoryRange(
      vulkan_device, builtin_index_buffer_memory_, upload_memory_type);
  dfn.vkUnmapMemory(device, builtin_index_buffer_upload_memory_);

  // Schedule uploading in the first submission.
  builtin_index_buffer_upload_submission_ = UINT64_MAX;
  return true;
}

void* VulkanPrimitiveProcessor::RequestHostConvertedIndexBufferForCurrentFrame(
    xenos::IndexFormat format, uint32_t index_count, bool coalign_for_simd,
    uint32_t coalignment_original_address, size_t& backend_handle_out) {
  size_t index_size = format == xenos::IndexFormat::kInt16 ? sizeof(uint16_t)
                                                           : sizeof(uint32_t);
  VkBuffer buffer;
  VkDeviceSize offset;
  uint8_t* mapping = frame_index_buffer_pool_->Request(
      command_processor_.GetCurrentFrame(),
      index_size * index_count +
          (coalign_for_simd ? XE_GPU_PRIMITIVE_PROCESSOR_SIMD_SIZE : 0),
      index_size, buffer, offset);
  if (!mapping) {
    return nullptr;
  }
  if (coalign_for_simd) {
    ptrdiff_t coalignment_offset =
        GetSimdCoalignmentOffset(mapping, coalignment_original_address);
    mapping += coalignment_offset;
    offset = VkDeviceSize(offset + coalignment_offset);
  }
  backend_handle_out = frame_index_buffers_.size();
  frame_index_buffers_.emplace_back(buffer, offset);
  return mapping;
}

}  // namespace vulkan
}  // namespace gpu
}  // namespace xe
