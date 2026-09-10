/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2025 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_VULKAN_VULKAN_DESCRIPTOR_POOL_CHAIN_H_
#define XENIA_UI_VULKAN_VULKAN_DESCRIPTOR_POOL_CHAIN_H_

#include <cstdint>
#include <vector>

#include "xenia/ui/vulkan/vulkan_device.h"

namespace xe {
namespace ui {
namespace vulkan {

// A chain of descriptor pools that rotates based on submission index,
// avoiding mid-frame GPU stalls when pools are exhausted.
// Similar to D3D12DescriptorHeapPool but for Vulkan's descriptor model.
class VulkanDescriptorPoolChain {
 public:
  VulkanDescriptorPoolChain(const VulkanDevice* device,
                            VkDescriptorPoolCreateFlags flags,
                            uint32_t max_sets_per_pool,
                            const VkDescriptorPoolSize* pool_sizes,
                            uint32_t pool_size_count,
                            VkDescriptorSetLayout set_layout);
  ~VulkanDescriptorPoolChain();

  // Reclaim pools that have been fully processed by the GPU.
  // Call this when submissions complete (e.g., after fence signaled).
  void Reclaim(uint64_t completed_submission_index);

  // Clear all pools. Must only be called when GPU is idle.
  void ClearCache();

  // Allocate a descriptor set from the chain.
  // Returns VK_NULL_HANDLE on failure.
  // The submission_index should be the current frame/submission being built.
  VkDescriptorSet Allocate(uint64_t submission_index);

  // Mark current pool as submitted and ready for reclaim after GPU completes.
  // Call this when submitting a command buffer.
  void EndSubmission(uint64_t submission_index);

 private:
  struct Pool {
    VkDescriptorPool pool;
    uint64_t last_submission_index;
    uint32_t allocated_sets;
    Pool* next;
  };

  Pool* CreatePool();
  void DestroyPool(Pool* pool);

  const VulkanDevice* device_;
  VkDescriptorPoolCreateFlags flags_;
  uint32_t max_sets_per_pool_;
  std::vector<VkDescriptorPoolSize> pool_sizes_;
  VkDescriptorSetLayout set_layout_;

  // Pools with free space, first is current.
  Pool* writable_first_ = nullptr;
  Pool* writable_last_ = nullptr;

  // Pools waiting for GPU to finish.
  Pool* submitted_first_ = nullptr;
  Pool* submitted_last_ = nullptr;
};

}  // namespace vulkan
}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_VULKAN_VULKAN_DESCRIPTOR_POOL_CHAIN_H_
