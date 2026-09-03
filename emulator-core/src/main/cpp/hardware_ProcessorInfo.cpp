// Created by xendroid on 2025/7/21.
// SPDX-License-Identifier: WTFPL
#include <stdlib.h>
#include <jni.h>
#include <vector>
#include <vulkan/vulkan.h>

extern "C" JNIEXPORT jstring JNICALL  Java_xendroid_hardware_ProcessorInfo_gpu_1get_1physical_1device_1name_1vk(JNIEnv*  env,jclass cls){

    VkApplicationInfo appinfo = {};
    appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appinfo.pNext = nullptr;
    appinfo.pApplicationName = "aps3e-cfg-test";
    appinfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appinfo.pEngineName = "nul";
    appinfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appinfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo inst_create_info = {};
    inst_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_create_info.pApplicationInfo = &appinfo;

    VkInstance inst;
    if (vkCreateInstance(&inst_create_info, nullptr, &inst)!= VK_SUCCESS) {
        return nullptr;
    }

    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(inst, &physicalDeviceCount, nullptr);

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(inst, &physicalDeviceCount, physicalDevices.data());

    if(physicalDevices.empty()){
        vkDestroyInstance(inst, nullptr);
        return nullptr;
    }

    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevices[0], &physicalDeviceProperties);

    vkDestroyInstance(inst, nullptr);

    return env->NewStringUTF(physicalDeviceProperties.deviceName);
}
