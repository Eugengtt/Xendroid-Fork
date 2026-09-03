
# Native build for compose (arm64-v8a only). See ../../../BUILD.md for the
# toolchain requirements (CMake version is pinned via cmake.dir in
# local.properties and selected by the Gradle externalNativeBuild block).
cmake_minimum_required(VERSION 3.30.3)

project(xendroid LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Global compile options applied to every target in this build.
add_compile_options(-march=armv8-a)
add_compile_options(-stdlib=libc++)
add_compile_options(-fno-omit-frame-pointer)
add_compile_options(-funwind-tables)

# 16 KB max page size: required for Android 15+ on arm64.
add_link_options(-Wl,-z,max-page-size=16384)

add_library(hardware_ProcessorInfo SHARED hardware_ProcessorInfo.cpp)
target_link_libraries(hardware_ProcessorInfo PRIVATE vulkan)

add_library(emu SHARED)
set_target_properties(emu PROPERTIES OUTPUT_NAME "e")

# Hide the static dependencies' symbols; only the JNI entry points are exported.
target_link_options(emu PRIVATE -Wl,--exclude-libs,ALL)

target_sources(emu
        PRIVATE
        aarch64_disasm.cpp
        xendroid.cpp
        xendroid_emu.cpp
        emulator.cpp
        emulator_xendroid.cpp
        xe_aaudio_audio_driver.cpp
        xe_aaudio_audio_system.cpp
        xe_android_disc_swap.cpp
        xe_android_hid.cpp
        xe_android_input_driver.cpp
        xe_android_message_box.cpp
        xe_android_text_input.cpp
        xe_android_util.cpp
        xe_opensles_audio_driver.cpp
        xe_opensles_audio_system.cpp
        xe_system_xendroid.cpp
        xenia/src/xenia/app/emulator_window.cc
        xenia/src/xenia/ui/file_picker_android.cc
        xenia/src/xenia/ui/surface_android.cc
        cpuinfo.cpp
        vkapi.cpp
        vkutil.cpp
)

# Pre-flight: the GPU/UI shaders are compiled to SPIR-V at build time by
# xenia/tools/build/compile_shader_spirv.py, which needs these host tools.
# Validate them at configure time so a missing dependency is a clear, actionable
# error instead of a confusing Python failure deep inside the Ninja build.
set(_xe_vk_hint_dirs)
if(DEFINED ENV{VULKAN_SDK})
    list(APPEND _xe_vk_hint_dirs "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin")
endif()
foreach(_tool glslangValidator spirv-opt spirv-dis)
    string(MAKE_C_IDENTIFIER "XE_TOOL_${_tool}" _tool_var)
    find_program(${_tool_var} NAMES ${_tool} HINTS ${_xe_vk_hint_dirs})
    if(NOT ${_tool_var})
        message(FATAL_ERROR
            "Shader toolchain: '${_tool}' not found.\n"
            "  It compiles the GPU/UI shaders to SPIR-V at build time.\n"
            "  Windows/macOS: install the Vulkan SDK (https://vulkan.lunarg.com) -- "
            "it sets VULKAN_SDK and ships glslangValidator/spirv-opt/spirv-dis.\n"
            "  Linux (Debian/Ubuntu): sudo apt install glslang-tools spirv-tools\n"
            "  Then ensure the tools are on PATH or under $VULKAN_SDK/bin. See BUILD.md.")
    endif()
endforeach()

# Regenerate the Vulkan SPIR-V shader bytecode at configure time. Edge's in-tree
# shader pipeline (xenia-shader-cc + slangc) can't run in the arm64 NDK cross
# build, so gpu/vulkan and ui/vulkan have their edge shader rules guarded off on
# Android (see those CMakeLists) and consume bytecode produced here by the host
# Python + system glslang toolchain validated just above. Incremental, so it is
# a near no-op once generated. Output dirs are git-ignored generated headers.
find_program(XE_PYTHON3 NAMES python3 python)
if(NOT XE_PYTHON3)
    message(FATAL_ERROR "python3 not found; required to generate SPIR-V shader bytecode.")
endif()
set(_xe_shader_root "${CMAKE_CURRENT_SOURCE_DIR}/xenia/src/xenia")
execute_process(
    COMMAND "${XE_PYTHON3}"
            "${CMAKE_CURRENT_SOURCE_DIR}/xenia/tools/build/gen_android_spirv.py"
            "${_xe_shader_root}/gpu/shaders"
            "${_xe_shader_root}/ui/shaders"
    RESULT_VARIABLE _xe_spirv_result
    OUTPUT_VARIABLE _xe_spirv_out)
message(STATUS "XenDroid SPIR-V shader bytecode: ${_xe_spirv_out}")
if(NOT _xe_spirv_result EQUAL 0)
    message(FATAL_ERROR "SPIR-V shader bytecode generation failed (exit ${_xe_spirv_result}).")
endif()

add_subdirectory(xenia EXCLUDE_FROM_ALL)

add_subdirectory(xenia/third_party/libadrenotools EXCLUDE_FROM_ALL)

target_include_directories(emu PRIVATE
        xenia
        xenia/src
        xenia/third_party
        xenia/..
)

target_link_libraries(emu
        PRIVATE
        xenia-apu
        xenia-apu-nop
        xenia-base
        xenia-core
        xenia-cpu
        xenia-cpu-backend-a64
        xenia-gpu
        xenia-gpu-null
        xenia-gpu-vulkan
        xenia-hid
        xenia-hid-nop
        xenia-kernel
        xenia-ui
        xenia-ui-vulkan
        xenia-vfs
        aes_128
        capstone
        fmt
        dxbc
        discord-rpc
        glslang-spirv
        imgui
        libavcodec
        libavutil
        libavformat
        mspack
        snappy
        xxhash
        android
        dl
        log
        vulkan
        OpenSLES
        aaudio
        z
        pugixml
        xenia-patcher
        adrenotools
)
