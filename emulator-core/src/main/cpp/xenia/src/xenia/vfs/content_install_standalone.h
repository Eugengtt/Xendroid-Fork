// SPDX-License-Identifier: WTFPL
#ifndef XENIA_VFS_CONTENT_INSTALL_STANDALONE_H_
#define XENIA_VFS_CONTENT_INSTALL_STANDALONE_H_

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "xenia/xbox.h"  // X_STATUS

namespace xe {
namespace vfs {

// Live progress for a running standalone install (file-static atomics back the
// JNI getter; concurrent reads are safe). current/total are payload bytes.
struct ContentProgress {
  std::atomic<uint64_t> current{0};
  std::atomic<uint64_t> total{0};
};

// Extract an STFS/SVOD package's inner file tree into the on-disk content tree,
// kernel-free (no Emulator / kernel_state). Placement is content-type aware:
// profiles go under kDashboardID with the account XUID as the leaf (so
// ProfileManager discovers them); everything else (DLC, title updates,
// arcade/GoD) stays under machine XUID 0. Returns X_STATUS (0 == success).
// Blocking VFS walk -- caller MUST run off the main thread.
X_STATUS InstallContentPackageStandalone(
    const std::filesystem::path& src_path,
    const std::filesystem::path& content_root, ContentProgress& progress);

struct DiscContentItem {
  std::string inner_path;  // path inside the disc image, e.g. \content\...\pkg
  std::string display_name;
  uint32_t title_id;
  uint32_t content_type;
  uint64_t size;
};

// Enumerate the installable packages a disc image carries under \content\, the
// payload a mandatory-install title (GTA V and friends) copies to the HDD before
// it will run. Returns an empty vector for a disc with no such tree.
// Blocking disc walk -- caller MUST run off the main thread.
std::vector<DiscContentItem> ListDiscContent(
    const std::filesystem::path& disc_path);

// Install one package named by ListDiscContent's inner_path. The package is
// staged out of the disc image into scratch_dir first, because the container
// device reads a host file, then installed through the standalone path above
// and the staged copy deleted. Returns X_STATUS (0 == success).
// Blocking -- caller MUST run off the main thread.
X_STATUS InstallDiscContentPackage(const std::filesystem::path& disc_path,
                                   const std::string& inner_path,
                                   const std::filesystem::path& content_root,
                                   const std::filesystem::path& scratch_dir,
                                   ContentProgress& progress);

struct InstalledContentItem {
  std::string pkg_dir;
  std::string display_name;
  uint64_t size;
};

// Enumerate installed packages of content_type (DLC 0x2, Title Update 0xB0000)
// under XUID 0, mirroring the standalone installer's on-disk layout. Returns an
// empty vector (not an error) when the tree is absent.
std::vector<InstalledContentItem> ListInstalledContent(
    const std::filesystem::path& content_root, uint32_t title_id,
    uint32_t content_type);

// Remove one installed package's data dir + its .header sidecar. Returns
// X_STATUS (0 == success).
X_STATUS DeleteInstalledContent(const std::filesystem::path& content_root,
                                uint32_t title_id, uint32_t content_type,
                                const std::string& pkg_dir);

}  // namespace vfs
}  // namespace xe

#endif  // XENIA_VFS_CONTENT_INSTALL_STANDALONE_H_
