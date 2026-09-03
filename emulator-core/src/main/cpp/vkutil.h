
// SPDX-License-Identifier: WTFPL
// Created by xendroid on 2025/5/31.
//

#ifndef APS3E_VKUTIL_H
#define APS3E_VKUTIL_H

#include "vkapi.h"
#include <optional>
#include <vector>

std::optional<VkInstance> vk_create_instance(const char * name);

int vk_get_physical_device_count(VkInstance inst);

std::optional<VkPhysicalDevice> vk_get_physical_device(VkInstance inst, int index=0);

VkPhysicalDeviceProperties vk_get_physical_device_properties(VkPhysicalDevice dev);

VkPhysicalDeviceLimits vk_get_physical_device_limits(VkPhysicalDevice dev);

std::vector<VkExtensionProperties> vk_get_physical_device_extension_properties(VkPhysicalDevice dev);

void vk_destroy_instance(VkInstance inst);

int vk_get_queue_family_count(VkPhysicalDevice dev);

VkQueueFamilyProperties vk_get_queue_family_properties(VkPhysicalDevice dev,int index);

std::optional<VkDevice> vk_create_device(VkPhysicalDevice pdev,uint32_t queueFamilyIndex,VkQueueFamilyProperties  props);

void vk_destroy_device(VkDevice dev);

std::optional<VkDescriptorSetLayout> vk_create_descriptor_set_layout(VkDevice dev,const std::vector<VkDescriptorSetLayoutBinding>& binds);

void vk_destroy_descriptor_set_layout(VkDevice dev,VkDescriptorSetLayout layout);

std::optional<VkPipelineLayout> vk_create_pipeline_layout(VkDevice dev,VkDescriptorSetLayout descriptor_set_layout);

std::optional<std::vector<uint32_t>> vk_compile_glsl_to_spv(VkDevice dev,const std::string& source,VkPhysicalDeviceLimits limits);

std::optional<VkShaderModule> vk_create_shader_module(VkDevice dev,const std::vector<uint32_t>& code);

void vk_destroy_shader_module(VkDevice dev,VkShaderModule module);

void vk_destroy_pipeline_layout(VkDevice dev,VkPipelineLayout layout);

std::optional <VkPipeline> vk_create_compute_pipeline(VkDevice dev,VkPipelineLayout layout,VkShaderModule module);

void  vk_destroy_pipeline(VkDevice dev,VkPipeline pipeline);

#endif //APS3E_VKUTIL_H
