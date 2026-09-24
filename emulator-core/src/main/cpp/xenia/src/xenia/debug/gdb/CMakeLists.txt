add_library(xenia-debug-gdb STATIC)
xe_platform_sources(xenia-debug-gdb ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(xenia-debug-gdb PUBLIC imgui xenia-base xenia-cpu xenia-ui)
if(MSVC)
  # Prefer size over speed — the GDB stub isn't performance-critical.
  target_compile_options(xenia-debug-gdb PRIVATE
    $<$<CONFIG:Release>:/Os>
  )
endif()
xe_target_defaults(xenia-debug-gdb)
