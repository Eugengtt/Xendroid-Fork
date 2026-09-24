// SPDX-License-Identifier: WTFPL
#ifndef XENIA_KERNEL_XAM_PROFILE_STANDALONE_H_
#define XENIA_KERNEL_XAM_PROFILE_STANDALONE_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "xenia/xbox.h"  // X_STATUS

namespace xe {
namespace kernel {
namespace xam {

struct StandaloneProfile {
  uint64_t xuid;
  std::string gamertag;
  uint32_t language;
  uint32_t country;
  bool has_avatar;
};

std::string CreateStandaloneProfile(const std::filesystem::path& content_root,
                                    const std::string& gamertag,
                                    uint32_t language, uint32_t country);

std::vector<StandaloneProfile> ListStandaloneProfiles(
    const std::filesystem::path& content_root);

X_STATUS RenameStandaloneProfile(const std::filesystem::path& content_root,
                                 uint64_t xuid, const std::string& gamertag,
                                 uint32_t language, uint32_t country);

}  // namespace xam
}  // namespace kernel
}  // namespace xe

#endif  // XENIA_KERNEL_XAM_PROFILE_STANDALONE_H_
