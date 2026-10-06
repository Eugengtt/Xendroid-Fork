/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2025 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/ui/vulkan/vulkan_descriptor_pool_chain.h"

#include "xenia/base/logging.h"

namespace xe {
namespace ui {
namespace vulkan {

VulkanDescriptorPoolChain::VulkanDescriptorPoolChain(
    const VulkanDevice* device, VkDescriptorPoolCreateFlags flags,
    uint32_t max_sets_per_pool, const VkDescriptorPoolSize* pool_sizes,
    uint32_t pool_size_count, VkDescriptorSetLayout set_layout)
    : device_(device),
      flags_(flags),
      max_sets_per_pool_(max_sets_per_pool),
      set_layout_(set_layout) {
  pool_sizes_.assign(pool_sizes, pool_sizes + pool_size_count);
}

VulkanDescriptorPoolChain::~VulkanDescriptorPoolChain() { ClearCache(); }

void VulkanDescriptorPoolChain::Reclaim(uint64_t completed_submission_index) {
  // Move pools that the GPU has finished using back to the writable list.
  while (submitted_first_) {
    if (submitted_first_->last_submission_index > completed_submission_index) {
      break;
    }
    // Reset the pool so it can be reused.
    const VulkanDevice::Functions& dfn = device_->functions();
    dfn.vkResetDescriptorPool(device_->device(), submitted_first_->pool, 0);
    submitted_first_->allocated_sets = 0;

    // Move to writable list.
    if (writable_last_) {
      writable_last_->next = submitted_first_;
    } else {
      writable_first_ = submitted_first_;
    }
    writable_last_ = submitted_first_;
    submitted_first_ = submitted_first_->next;
    writable_last_->next = nullptr;
  }
  if (!submitted_first_) {
    submitted_last_ = nullptr;
  }
}

void VulkanDescriptorPoolChain::ClearCache() {
  const VulkanDevice::Functions& dfn = device_->functions();
  VkDevice vk_device = device_->device();

  while (submitted_first_) {
    Pool* next = submitted_first_->next;
    DestroyPool(submitted_first_);
    submitted_first_ = next;
  }
  submitted_last_ = nullptr;

  while (writable_first_) {
    Pool* next = writable_first_->next;
    DestroyPool(writable_first_);
    writable_first_ = next;
  }
  writable_last_ = nullptr;
}

VkDescriptorSet VulkanDescriptorPoolChain::Allocate(uint64_t submission_index) {
  const VulkanDevice::Functions& dfn = device_->functions();
  VkDevice vk_device = device_->device();

  // If current pool is full, move to next writable pool or create new.
  if (writable_first_ &&
      writable_first_->allocated_sets >= max_sets_per_pool_) {
    // Current pool is full, move it to submitted list.
    if (submitted_last_) {
      submitted_last_->next = writable_first_;
    } else {
      submitted_first_ = writable_first_;
    }
    submitted_last_ = writable_first_;
    writable_first_ = writable_first_->next;
    submitted_last_->next = nullptr;
    if (!writable_first_) {
      writable_last_ = nullptr;
    }
  }

  // Create new pool if needed.
  if (!writable_first_) {
    writable_first_ = CreatePool();
    if (!writable_first_) {
      return VK_NULL_HANDLE;
    }
    writable_last_ = writable_first_;
  }

  // Allocate from current pool.
  VkDescriptorSetAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = writable_first_->pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &set_layout_;

  VkDescriptorSet descriptor_set;
  VkResult result =
      dfn.vkAllocateDescriptorSets(vk_device, &alloc_info, &descriptor_set);

  if (result != VK_SUCCESS) {
    // Pool exhausted unexpectedly, try creating a new pool.
    // Move current to submitted.
    if (submitted_last_) {
      submitted_last_->next = writable_first_;
    } else {
      submitted_first_ = writable_first_;
    }
    submitted_last_ = writable_first_;
    writable_first_ = writable_first_->next;
    submitted_last_->next = nullptr;
    if (!writable_first_) {
      writable_last_ = nullptr;
    }

    // Create new pool.
    writable_first_ = CreatePool();
    if (!writable_first_) {
      return VK_NULL_HANDLE;
    }
    writable_last_ = writable_first_;

    // Retry allocation.
    alloc_info.descriptorPool = writable_first_->pool;
    result =
        dfn.vkAllocateDescriptorSets(vk_device, &alloc_info, &descriptor_set);
    if (result != VK_SUCCESS) {
      XELOGE("VulkanDescriptorPoolChain: Failed to allocate descriptor set");
      return VK_NULL_HANDLE;
    }
  }

  writable_first_->allocated_sets++;
  writable_first_->last_submission_index = submission_index;
  return descriptor_set;
}

void VulkanDescriptorPoolChain::EndSubmission(uint64_t submission_index) {
  // Mark current pool with submission index for reclaim tracking.
  if (writable_first_) {
    writable_first_->last_submission_index = submission_index;
  }
}

VulkanDescriptorPoolChain::Pool* VulkanDescriptorPoolChain::CreatePool() {
  const VulkanDevice::Functions& dfn = device_->functions();
  VkDevice vk_device = device_->device();

  VkDescriptorPoolCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  create_info.flags = flags_;
  create_info.maxSets = max_sets_per_pool_;
  create_info.poolSizeCount = static_cast<uint32_t>(pool_sizes_.size());
  create_info.pPoolSizes = pool_sizes_.data();

  VkDescriptorPool vk_pool;
  if (dfn.vkCreateDescriptorPool(vk_device, &create_info, nullptr, &vk_pool) !=
      VK_SUCCESS) {
    XELOGE("VulkanDescriptorPoolChain: Failed to create descriptor pool");
    return nullptr;
  }

  Pool* pool = new Pool;
  pool->pool = vk_pool;
  pool->last_submission_index = 0;
  pool->allocated_sets = 0;
  pool->next = nullptr;
  return pool;
}

void VulkanDescriptorPoolChain::DestroyPool(Pool* pool) {
  const VulkanDevice::Functions& dfn = device_->functions();
  dfn.vkDestroyDescriptorPool(device_->device(), pool->pool, nullptr);
  delete pool;
}

}  // namespace vulkan
}  // namespace ui
}  // namespace xe
