add_library(xenia-ui-d3d12 STATIC)
xe_platform_sources(xenia-ui-d3d12 ${CMAKE_CURRENT_SOURCE_DIR})
xe_shader_rules_slang(xenia-ui-d3d12 ${CMAKE_CURRENT_SOURCE_DIR}/../shaders
                      TARGET dxil)
target_link_libraries(xenia-ui-d3d12 PUBLIC xenia-base xenia-ui)
xe_target_defaults(xenia-ui-d3d12)

if(XENIA_BUILD_MISC)
  add_executable(xenia-ui-window-d3d12-demo WIN32
    ${CMAKE_CURRENT_SOURCE_DIR}/../window_demo.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/d3d12_window_demo.cc
    ${PROJECT_SOURCE_DIR}/src/xenia/ui/windowed_app_main_qt.cc
  )
  target_link_libraries(xenia-ui-window-d3d12-demo PRIVATE
    xenia-apu xenia-base xenia-core xenia-cpu
    xenia-gpu xenia-hid xenia-hid-portal
    xenia-kernel xenia-patcher xenia-ui xenia-ui-d3d12 xenia-vfs
    aes_128 capstone dxbc fmt glslang-spirv imgui mspack snappy xxhash
  )
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_link_libraries(xenia-ui-window-d3d12-demo PRIVATE xenia-cpu-backend-x64)
  endif()
  xe_target_defaults(xenia-ui-window-d3d12-demo)
endif()
