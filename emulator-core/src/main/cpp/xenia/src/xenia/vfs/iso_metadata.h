/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_VFS_ISO_METADATA_H_
#define XENIA_VFS_ISO_METADATA_H_

#include <cstdint>
#include <filesystem>
#include <optional>

#include "xenia/vfs/xex_metadata.h"

namespace xe {
namespace vfs {

std::optional<XexMetadata> ExtractIsoMetadata(
    const std::filesystem::path& path);
std::optional<XexMetadata> ExtractIsoMetadata(const uint8_t* data, size_t size);

}  // namespace vfs
}  // namespace xe

#endif  // XENIA_VFS_ISO_METADATA_H_
