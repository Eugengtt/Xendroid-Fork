add_library(xenia-gpu-vulkan STATIC)
xe_platform_sources(xenia-gpu-vulkan ${CMAKE_CURRENT_SOURCE_DIR})
# XenDroid (Android): edge's shader pipeline builds via the host tool xenia-shader-cc
# (and slangc for .slang sources), which can't run in the arm64 NDK cross-build.
# Skip shader regeneration on Android and consume the committed SPIR-V bytecode headers
# under gpu/shaders/bytecode/vulkan_spirv/. See REBASE_EDGE_REPORT.md (build phase).
if(NOT ANDROID)
  xe_shader_rules_spirv(xenia-gpu-vulkan ${CMAKE_CURRENT_SOURCE_DIR}/../shaders)
  xe_shader_rules_slang(xenia-gpu-vulkan ${CMAKE_CURRENT_SOURCE_DIR}/../shaders
                        TARGET spirv)
endif()
target_include_directories(xenia-gpu-vulkan PRIVATE
  ${PROJECT_SOURCE_DIR}/third_party/Vulkan-Headers/include
  ${PROJECT_SOURCE_DIR}/third_party/glslang
)
if(WIN32 AND DEFINED ENV{VULKAN_SDK})
  target_include_directories(xenia-gpu-vulkan PRIVATE "$ENV{VULKAN_SDK}/Include")
endif()
target_link_libraries(xenia-gpu-vulkan PUBLIC
  fmt glslang-spirv xxhash xenia-base xenia-gpu xenia-ui xenia-ui-vulkan
)
xe_target_defaults(xenia-gpu-vulkan)

if(XENIA_BUILD_MISC)
  add_executable(xenia-gpu-vulkan-trace-viewer
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_trace_viewer_main.cc
    ${PROJECT_SOURCE_DIR}/src/xenia/ui/windowed_app_main_qt.cc
  )
  target_include_directories(xenia-gpu-vulkan-trace-viewer PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/Vulkan-Headers/include
    ${PROJECT_SOURCE_DIR}/third_party/glslang
  )
  target_link_libraries(xenia-gpu-vulkan-trace-viewer PRIVATE
    xenia-apu xenia-apu-nop xenia-base xenia-core xenia-cpu
    xenia-gpu xenia-gpu-vulkan xenia-hid xenia-hid-nop xenia-hid-portal
    xenia-kernel xenia-patcher xenia-ui xenia-ui-vulkan xenia-vfs
    aes_128 capstone fmt glslang-spirv imgui libavcodec libavutil mspack snappy xxhash
  )
  if(XE_TARGET_X86_64)
    target_link_libraries(xenia-gpu-vulkan-trace-viewer PRIVATE xenia-cpu-backend-x64)
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(xenia-gpu-vulkan-trace-viewer PRIVATE X11 xcb X11-xcb xenia-apu-sdl)
  endif()
  xe_target_defaults(xenia-gpu-vulkan-trace-viewer)

  # Vulkan trace dump
  add_executable(xenia-gpu-vulkan-trace-dump
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_trace_dump_main.cc
  )
  if(WIN32)
    target_sources(xenia-gpu-vulkan-trace-dump PRIVATE
      ${PROJECT_SOURCE_DIR}/src/xenia/base/console_app_main_win.cc)
  else()
    target_sources(xenia-gpu-vulkan-trace-dump PRIVATE
      ${PROJECT_SOURCE_DIR}/src/xenia/base/console_app_main_posix.cc)
  endif()
  target_include_directories(xenia-gpu-vulkan-trace-dump PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/Vulkan-Headers/include
    ${PROJECT_SOURCE_DIR}/third_party/glslang
  )
  target_link_libraries(xenia-gpu-vulkan-trace-dump PRIVATE
    xenia-apu xenia-apu-nop xenia-base xenia-core xenia-cpu
    xenia-gpu xenia-gpu-vulkan xenia-hid xenia-hid-nop xenia-hid-portal
    xenia-kernel xenia-patcher xenia-ui xenia-ui-vulkan xenia-vfs
    aes_128 capstone fmt glslang-spirv imgui libavcodec libavutil mspack snappy xxhash
  )
  if(XE_TARGET_X86_64)
    target_link_libraries(xenia-gpu-vulkan-trace-dump PRIVATE xenia-cpu-backend-x64)
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(xenia-gpu-vulkan-trace-dump PRIVATE xenia-apu-sdl)
  endif()
  xe_target_defaults(xenia-gpu-vulkan-trace-dump)
endif()
