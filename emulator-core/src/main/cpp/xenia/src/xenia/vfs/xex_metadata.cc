/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/vfs/xex_metadata.h"

#include <cstring>
#include <fstream>
#include <vector>

#include "third_party/fmt/include/fmt/format.h"
#include "xenia/base/byte_order.h"
#include "xenia/kernel/util/xex2_info.h"
#include "xenia/kernel/util/xex2_util.h"

namespace xe {
namespace vfs {

namespace {

constexpr uint32_t kXex1Signature = 0x58455831;  // 'XEX1'
constexpr uint32_t kXex2Signature = 0x58455832;  // 'XEX2'

XexVersion ParseVersion(uint32_t value) {
  // xex2_version bit layout: major:4, minor:4, build:16, qfe:8
  XexVersion v;
  v.major = (value >> 28) & 0xF;
  v.minor = (value >> 24) & 0xF;
  v.build = (value >> 8) & 0xFFFF;
  v.qfe = value & 0xFF;
  return v;
}

}  // namespace

std::string XexVersion::ToString() const {
  return fmt::format("{}.{}.{}.{}", major, minor, build, qfe);
}

toml::table XexMetadata::ToToml() const {
  toml::table root;

  toml::table module_table;
  module_table.insert("name", module_name);
  switch (format) {
    case XexFormat::kXex1:
      module_table.insert("format", "XEX1");
      break;
    case XexFormat::kXex2:
      module_table.insert("format", "XEX2");
      break;
    default:
      module_table.insert("format", "unknown");
      break;
  }
  root.insert("module", std::move(module_table));

  toml::table exec_table;
  exec_table.insert("title_id", fmt::format("{:08X}", title_id));
  exec_table.insert("media_id", fmt::format("{:08X}", media_id));
  exec_table.insert("savegame_id", fmt::format("{:08X}", savegame_id));
  exec_table.insert("version", version.ToString());
  exec_table.insert("base_version", base_version.ToString());
  exec_table.insert("disc_number", static_cast<int64_t>(disc_number));
  exec_table.insert("disc_count", static_cast<int64_t>(disc_count));
  root.insert("execution_info", std::move(exec_table));

  if (ratings.present) {
    toml::table ratings_table;
    if (ratings.esrb != 0xFF) {
      ratings_table.insert("esrb", static_cast<int64_t>(ratings.esrb));
    }
    if (ratings.pegi != 0xFF) {
      ratings_table.insert("pegi", static_cast<int64_t>(ratings.pegi));
    }
    if (ratings.cero != 0xFF) {
      ratings_table.insert("cero", static_cast<int64_t>(ratings.cero));
    }
    if (ratings.usk != 0xFF) {
      ratings_table.insert("usk", static_cast<int64_t>(ratings.usk));
    }
    if (ratings.oflc_au != 0xFF) {
      ratings_table.insert("oflc_au", static_cast<int64_t>(ratings.oflc_au));
    }
    if (ratings.oflc_nz != 0xFF) {
      ratings_table.insert("oflc_nz", static_cast<int64_t>(ratings.oflc_nz));
    }
    if (!ratings_table.empty()) {
      root.insert("ratings", std::move(ratings_table));
    }
  }

  return root;
}

std::optional<XexMetadata> ExtractXexMetadata(const uint8_t* data,
                                              size_t size) {
  if (size < sizeof(xex2_header)) {
    return std::nullopt;
  }

  auto header = reinterpret_cast<const xex2_header*>(data);

  XexFormat format;
  uint32_t magic = header->magic;
  if (magic == kXex2Signature) {
    format = XexFormat::kXex2;
  } else if (magic == kXex1Signature) {
    format = XexFormat::kXex1;
  } else {
    return std::nullopt;
  }

  uint32_t header_size = header->header_size;
  if (header_size < sizeof(xex2_header) || header_size > size) {
    return std::nullopt;
  }

  XexMetadata metadata;
  metadata.format = format;

  xex2_opt_execution_info* exec_info = nullptr;
  if (GetXexOptHeader(header, XEX_HEADER_EXECUTION_INFO, &exec_info) &&
      exec_info) {
    metadata.title_id = exec_info->title_id;
    metadata.media_id = exec_info->media_id;
    metadata.savegame_id = exec_info->savegame_id;
    metadata.version = ParseVersion(exec_info->version_value);
    metadata.base_version = ParseVersion(exec_info->base_version_value);
    metadata.disc_number = exec_info->disc_number;
    metadata.disc_count = exec_info->disc_count;
  }

  xex2_opt_original_pe_name* pe_name = nullptr;
  if (GetXexOptHeader(header, XEX_HEADER_ORIGINAL_PE_NAME, &pe_name) &&
      pe_name) {
    // Size field includes itself.
    uint32_t name_size = pe_name->size;
    if (name_size > sizeof(uint32_t)) {
      size_t name_len = name_size - sizeof(uint32_t);
      metadata.module_name = std::string(pe_name->name, name_len);
      while (!metadata.module_name.empty() &&
             metadata.module_name.back() == '\0') {
        metadata.module_name.pop_back();
      }
    }
  }

  xex2_game_ratings_t* ratings = nullptr;
  if (GetXexOptHeader(header, XEX_HEADER_GAME_RATINGS, &ratings) && ratings) {
    metadata.ratings.present = true;
    metadata.ratings.esrb = static_cast<uint8_t>(ratings->esrb);
    metadata.ratings.pegi = static_cast<uint8_t>(ratings->pegi);
    metadata.ratings.cero = static_cast<uint8_t>(ratings->cero);
    metadata.ratings.usk = static_cast<uint8_t>(ratings->usk);
    metadata.ratings.oflc_au = static_cast<uint8_t>(ratings->oflcau);
    metadata.ratings.oflc_nz = static_cast<uint8_t>(ratings->oflcnz);
  }

  return metadata;
}

std::optional<XexMetadata> ExtractXexMetadata(
    const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }

  xex2_header base_header;
  file.read(reinterpret_cast<char*>(&base_header), sizeof(base_header));
  if (!file ||
      file.gcount() < static_cast<std::streamsize>(sizeof(base_header))) {
    return std::nullopt;
  }

  uint32_t magic = base_header.magic;
  if (magic != kXex2Signature && magic != kXex1Signature) {
    return std::nullopt;
  }

  uint32_t header_size = base_header.header_size;
  if (header_size < sizeof(xex2_header)) {
    return std::nullopt;
  }

  std::vector<uint8_t> header_data(header_size);
  file.seekg(0);
  file.read(reinterpret_cast<char*>(header_data.data()), header_size);
  if (!file || file.gcount() < static_cast<std::streamsize>(header_size)) {
    return std::nullopt;
  }

  return ExtractXexMetadata(header_data.data(), header_data.size());
}

}  // namespace vfs
}  // namespace xe
