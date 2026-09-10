add_library(xenia-vfs STATIC)
xe_platform_sources(xenia-vfs ${CMAKE_CURRENT_SOURCE_DIR} RECURSIVE)
# Exclude vfs_dump.cc and xex_metadata_main.cc (these are their own executables)
file(GLOB _vfs_exclude
  "${CMAKE_CURRENT_SOURCE_DIR}/vfs_dump.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/xex_metadata_main.cc"
)
get_target_property(_vfs_srcs xenia-vfs SOURCES)
if(_vfs_exclude AND _vfs_srcs)
  list(REMOVE_ITEM _vfs_srcs ${_vfs_exclude})
  set_target_properties(xenia-vfs PROPERTIES SOURCES "${_vfs_srcs}")
endif()
target_link_libraries(xenia-vfs PUBLIC xenia-base zstd zarchive)
xe_target_defaults(xenia-vfs)

if(XENIA_BUILD_MISC)
  # xenia-xex-metadata: standalone console tool that dumps XEX metadata
  add_executable(xenia-xex-metadata
    ${CMAKE_CURRENT_SOURCE_DIR}/xex_metadata_main.cc
  )
  if(WIN32)
    target_sources(xenia-xex-metadata PRIVATE
      ${PROJECT_SOURCE_DIR}/src/xenia/base/console_app_main_win.cc)
  else()
    target_sources(xenia-xex-metadata PRIVATE
      ${PROJECT_SOURCE_DIR}/src/xenia/base/console_app_main_posix.cc)
  endif()
  target_link_libraries(xenia-xex-metadata PRIVATE fmt xenia-base xenia-vfs)
  xe_target_defaults(xenia-xex-metadata)
  set_target_properties(xenia-xex-metadata PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/host_tools")

  # VFS dump tool
  add_executable(xenia-vfs-dump
    ${CMAKE_CURRENT_SOURCE_DIR}/vfs_dump.cc
  )
  if(WIN32)
    target_sources(xenia-vfs-dump PRIVATE
      ${PROJECT_SOURCE_DIR}/src/xenia/base/console_app_main_win.cc)
  else()
    target_sources(xenia-vfs-dump PRIVATE
      ${PROJECT_SOURCE_DIR}/src/xenia/base/console_app_main_posix.cc)
  endif()
  target_link_libraries(xenia-vfs-dump PRIVATE fmt xenia-base xenia-vfs)
  xe_target_defaults(xenia-vfs-dump)
endif()

if(XENIA_BUILD_TESTS)
  add_subdirectory(testing)
endif()
