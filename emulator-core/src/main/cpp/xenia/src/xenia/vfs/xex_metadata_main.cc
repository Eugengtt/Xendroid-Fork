/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>

#include "fmt/format.h"
#include "xenia/base/console_app_main.h"
#include "xenia/base/cvar.h"
#include "xenia/base/filesystem.h"
#include "xenia/base/logging.h"
#include "xenia/vfs/iso_metadata.h"
#include "xenia/vfs/stfs_metadata.h"
#include "xenia/vfs/xex_metadata.h"
#include "xenia/vfs/zar_metadata.h"

namespace xe {
namespace vfs {

DEFINE_transient_path(
    target, "", "Path to game file or directory to scan for bootable files.",
    "General");

namespace {

std::string ToLower(const std::string& s) {
  std::string result = s;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

// Result of extracting metadata from a file.
struct ExtractedMetadata {
  std::filesystem::path path;
  std::string format;    // "xex", "stfs", "iso", "zar"
  std::string title_id;  // hex string
  std::string module_name;
  std::string display_name;  // For STFS containers
  toml::table full_metadata;
};

// Try to extract metadata from a file. Returns nullopt if not a valid game
// file.
std::optional<ExtractedMetadata> TryExtractMetadata(
    const std::filesystem::path& path) {
  // Skip directories - some extract functions throw on directories.
  std::error_code ec;
  if (std::filesystem::is_directory(path, ec) || ec) {
    return std::nullopt;
  }

  try {
    std::string ext = ToLower(path.extension().string());

    // Try to extract based on extension hint.
    if (ext == ".iso") {
      if (auto m = ExtractIsoMetadata(path)) {
        return ExtractedMetadata{
            path,           "iso", fmt::format("{:08X}", m->title_id),
            m->module_name, "",    m->ToToml()};
      }
    } else if (ext == ".zar") {
      if (auto m = ExtractZarMetadata(path)) {
        return ExtractedMetadata{
            path,           "zar", fmt::format("{:08X}", m->title_id),
            m->module_name, "",    m->ToToml()};
      }
    } else if (ext == ".xex") {
      if (auto m = ExtractXexMetadata(path)) {
        return ExtractedMetadata{
            path,           "xex", fmt::format("{:08X}", m->title_id),
            m->module_name, "",    m->ToToml()};
      }
    }

    // Unknown extension or extension hint failed - try all formats.
    // Try XEX first (smallest header check).
    if (auto m = ExtractXexMetadata(path)) {
      return ExtractedMetadata{
          path,           "xex", fmt::format("{:08X}", m->title_id),
          m->module_name, "",    m->ToToml()};
    }
    // Try STFS/SVOD container.
    if (auto m = ExtractStfsMetadata(path)) {
      return ExtractedMetadata{
          path, "stfs",          fmt::format("{:08X}", m->title_id),
          "",   m->display_name, m->ToToml()};
    }
    // Try ZAR.
    if (auto m = ExtractZarMetadata(path)) {
      return ExtractedMetadata{
          path,           "zar", fmt::format("{:08X}", m->title_id),
          m->module_name, "",    m->ToToml()};
    }
    // Try ISO.
    if (auto m = ExtractIsoMetadata(path)) {
      return ExtractedMetadata{
          path,           "iso", fmt::format("{:08X}", m->title_id),
          m->module_name, "",    m->ToToml()};
    }
  } catch (const std::exception&) {
    // Silently ignore files that cause exceptions during parsing.
  }

  return std::nullopt;
}

// Output a single file's full metadata.
int OutputSingleFile(const std::filesystem::path& path) {
  auto extracted = TryExtractMetadata(path);
  if (!extracted) {
    XELOGE("Failed to extract metadata from: {}", xe::path_to_utf8(path));
    return 1;
  }

  toml::table root;
  root.insert("source_format", extracted->format);
  for (const auto& [key, value] : extracted->full_metadata) {
    root.insert(key, value);
  }
  std::cout << root << std::endl;
  return 0;
}

// Scan a directory recursively for bootable game files.
int ScanDirectory(const std::filesystem::path& dir) {
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec) || ec) {
    XELOGE("Path is not a directory: {}", xe::path_to_utf8(dir));
    return 1;
  }

  std::vector<ExtractedMetadata> found_files;
  size_t files_scanned = 0;

  XELOGI("Scanning directory: {}", xe::path_to_utf8(dir));

  // Use iterator with proper error handling.
  auto options = std::filesystem::directory_options::skip_permission_denied;
  auto it = std::filesystem::recursive_directory_iterator(dir, options, ec);
  auto end = std::filesystem::recursive_directory_iterator();

  if (ec) {
    XELOGE("Failed to open directory: {}", ec.message());
    return 1;
  }

  while (it != end) {
    const auto& entry = *it;

    // Check if it's a regular file.
    bool is_file = entry.is_regular_file(ec);
    if (!ec && is_file) {
      files_scanned++;

      if (auto extracted = TryExtractMetadata(entry.path())) {
        XELOGI("Found: {} [{}] {}", xe::path_to_utf8(extracted->path),
               extracted->format, extracted->title_id);
        found_files.push_back(std::move(*extracted));
      }
    }

    // Advance iterator with error handling.
    it.increment(ec);
    if (ec) {
      XELOGW("Error accessing: {}", ec.message());
      ec.clear();
    }
  }

  // Output consolidated report as TOML.
  std::cout << "# Xenia Game File Scan Report\n";
  std::cout << "# Directory: " << xe::path_to_utf8(dir) << "\n";
  std::cout << "# Files scanned: " << files_scanned << "\n";
  std::cout << "# Bootable files found: " << found_files.size() << "\n\n";

  toml::table report;
  report.insert("scan_directory", xe::path_to_utf8(dir));
  report.insert("files_scanned", static_cast<int64_t>(files_scanned));
  report.insert("bootable_files_found",
                static_cast<int64_t>(found_files.size()));

  toml::array files_array;
  for (const auto& file : found_files) {
    toml::table file_entry;
    file_entry.insert("path", xe::path_to_utf8(file.path));
    file_entry.insert("format", file.format);
    file_entry.insert("title_id", file.title_id);
    if (!file.module_name.empty()) {
      file_entry.insert("module_name", file.module_name);
    }
    if (!file.display_name.empty()) {
      file_entry.insert("display_name", file.display_name);
    }
    files_array.push_back(file_entry);
  }
  report.insert("files", files_array);

  std::cout << report << std::endl;

  XELOGI("Scan complete. Found {} bootable files out of {} files scanned.",
         found_files.size(), files_scanned);

  return found_files.empty() ? 1 : 0;
}

}  // namespace

int xex_metadata_main(const std::vector<std::string>& args) {
  if (cvars::target.empty()) {
    XELOGE("Usage: {} <path-to-game-file-or-directory>", args[0]);
    XELOGE("");
    XELOGE("Supported formats: XEX, ISO, ZAR, STFS/SVOD containers");
    XELOGE("");
    XELOGE("If a directory is provided, it will be scanned recursively for");
    XELOGE("bootable game files.");
    return 1;
  }

  // Auto-detect: if target is a directory, scan it; otherwise treat as file.
  if (std::filesystem::is_directory(cvars::target)) {
    return ScanDirectory(cvars::target);
  }

  return OutputSingleFile(cvars::target);
}

}  // namespace vfs
}  // namespace xe

XE_DEFINE_CONSOLE_APP("xenia-xex-metadata", xe::vfs::xex_metadata_main,
                      "<path-to-game-file-or-directory>", "target");
