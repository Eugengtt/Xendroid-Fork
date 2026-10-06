// SPDX-License-Identifier: WTFPL

#ifndef xendroid_XE_ANDROID_DISC_SWAP_H
#define xendroid_XE_ANDROID_DISC_SWAP_H

#include <cstdint>
#include <string>
#include <vector>

namespace xendroid {

// Compose polls for one of these and answers it.
struct PendingDiscSwap {
  uint64_t id = 0;
  std::string message;
  bool is_error = false;
  uint32_t disc_number = 0;
  std::vector<std::string> disc_labels;
  std::vector<std::string> disc_paths;
};

// Until this is called the emulator keeps its headless behaviour.
void InstallDiscSwapProvider();

// The discs the host offers, in presentation order. Set before boot.
void SetKnownDiscs(std::vector<std::string> labels,
                   std::vector<std::string> paths);

// False when nothing is pending.
bool PeekDiscSwapRequest(PendingDiscSwap& out_request);

// Stale ids are ignored, so a late reply cannot corrupt a newer request.
void SubmitDiscSwap(uint64_t id, bool accepted, const std::string& path_utf8);

// Fails every waiting request; later ones are served normally again.
void CancelAllDiscSwap();

}  // namespace xendroid

#endif  // xendroid_XE_ANDROID_DISC_SWAP_H
