/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/vfs/iso_metadata.h"

#include "xenia/base/mapped_memory.h"
#include "xenia/vfs/gdfx_util.h"

namespace xe {
namespace vfs {

std::optional<XexMetadata> ExtractIsoMetadata(const uint8_t* data,
                                              size_t size) {
  // Find the game partition.
  auto partition = GdfxFindPartition(data, size);
  if (!partition) {
    return std::nullopt;
  }

  // Find default.xex in the root directory.
  auto location = GdfxFindFile(data, size, *partition, "default.xex");
  if (!location) {
    return std::nullopt;
  }

  // Validate the file location.
  if (location->offset + location->length > size) {
    return std::nullopt;
  }

  // Extract XEX metadata from the file data.
  return ExtractXexMetadata(data + location->offset, location->length);
}

std::optional<XexMetadata> ExtractIsoMetadata(
    const std::filesystem::path& path) {
  // Memory-map the ISO for efficient access.
  auto mmap = xe::MappedMemory::Open(path, xe::MappedMemory::Mode::kRead);
  if (!mmap) {
    return std::nullopt;
  }

  return ExtractIsoMetadata(mmap->data(), mmap->size());
}

}  // namespace vfs
}  // namespace xe
