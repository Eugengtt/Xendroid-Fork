// SPDX-License-Identifier: WTFPL

#ifndef xendroid_XE_ANDROID_MESSAGE_BOX_H
#define xendroid_XE_ANDROID_MESSAGE_BOX_H

#include <cstdint>
#include <string>
#include <vector>

namespace xendroid {

struct PendingMessageBox {
  uint64_t id = 0;
  std::string title;
  std::string text;
  std::vector<std::string> buttons;
  uint32_t active_button = 0;
  uint32_t flags = 0;
};

void InstallMessageBoxProvider();

bool PeekMessageBoxRequest(PendingMessageBox& out_request);

// Stale ids are ignored, so a late reply cannot corrupt a newer request.
void SubmitMessageBox(uint64_t id, uint32_t chosen_button);

// Fails every waiting request; later ones are served again (the emulator
// re-initializes in place on title relaunch).
void CancelAllMessageBox();

}  // namespace xendroid

#endif  // xendroid_XE_ANDROID_MESSAGE_BOX_H
