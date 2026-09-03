
// SPDX-License-Identifier: WTFPL
// Created by xendroid on 2025/5/29.
//
#include <dlfcn.h>
#include <string>
#include "vkapi.h"

#define VKFN(func) PFN_##func func##_
#include "vksym.h"
#undef VKFN
namespace {
    void* lib_handle = nullptr;
}
void vk_load(const char* lib_path,bool is_adreno_custom){
    if (lib_handle) return;
    lib_handle = dlopen(lib_path, RTLD_NOW);
#define VKFN(func) func##_=reinterpret_cast<PFN_##func>(dlsym(lib_handle, #func))
#include "vksym.h"
#undef VKFN
}
void vk_unload(){
    if (!lib_handle) return;
    dlclose(lib_handle);
    lib_handle = nullptr;
#define VKFN(func) func##_=nullptr
#include "vksym.h"
#undef VKFN
}