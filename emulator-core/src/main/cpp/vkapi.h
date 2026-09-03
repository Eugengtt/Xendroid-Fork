
// SPDX-License-Identifier: WTFPL
// Created by xendroid on 2025/5/29.
//

#ifndef APS3E_VKAPI_H
#define APS3E_VKAPI_H

#define VK_USE_PLATFORM_ANDROID_KHR
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#define VKFN(func) extern PFN_##func func##_
#include "vksym.h"
#undef VKFN

void vk_load(const char* lib_path,bool is_adreno_custom=false);
void vk_unload();

#endif //APS3E_VKAPI_H
