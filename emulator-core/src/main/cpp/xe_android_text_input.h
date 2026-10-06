// SPDX-License-Identifier: WTFPL

#ifndef xendroid_XE_ANDROID_TEXT_INPUT_H
#define xendroid_XE_ANDROID_TEXT_INPUT_H

#include <cstdint>
#include <string>

namespace xendroid {

// A pending guest text-entry request. Compose polls for one and answers it.
struct PendingTextInput {
  uint64_t id = 0;
  std::string title;
  std::string description;
  std::string default_text;
  uint32_t max_length = 0;
  uint32_t flags = 0;
};

// Until this is called the emulator keeps its headless behaviour.
void InstallTextInputProvider();

// False when nothing is pending.
bool PeekTextInputRequest(PendingTextInput& out_request);

// Stale ids are ignored, so a late reply cannot corrupt a newer request.
void SubmitTextInput(uint64_t id, bool accepted, const std::string& text_utf8);

// Fails every waiting request. Later requests are served normally again: the
// emulator tears down and re-initializes in place on title relaunch.
void CancelAllTextInput();

}  // namespace xendroid

#endif  // xendroid_XE_ANDROID_TEXT_INPUT_H
