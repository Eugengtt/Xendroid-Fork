// SPDX-License-Identifier: WTFPL

#ifndef XENIA_UI_HOST_TEXT_INPUT_H_
#define XENIA_UI_HOST_TEXT_INPUT_H_

#include <cstdint>
#include <functional>
#include <string>

namespace xe {
namespace ui {

// A host-rendered replacement for the ImGui text-entry dialog. Nothing installs
// a provider by default, so builds that do not opt in are unaffected.

struct HostTextInputRequest {
  std::string title;
  std::string description;
  std::string default_text;
  uint32_t max_length = 0;  // UTF-16 code units, terminator excluded
  uint32_t flags = 0;       // raw guest flags, uninterpreted
};

struct HostTextInputResult {
  bool accepted = false;
  std::string text;
};

// Blocks until answered, on the thread that would otherwise sit in the ImGui
// dialog's fence wait. False means it could not be presented; treat as a
// dismissal.
using HostTextInputProvider =
    std::function<bool(const HostTextInputRequest&, HostTextInputResult&)>;

// Fails every blocked request. The kernel calls this before waiting on the
// dispatch thread, which could otherwise be parked inside a request.
using HostTextInputCanceller = std::function<void()>;

void SetHostTextInputProvider(HostTextInputProvider provider,
                              HostTextInputCanceller canceller);
bool HasHostTextInputProvider();
bool RequestHostTextInput(const HostTextInputRequest& request,
                          HostTextInputResult& out_result);
void CancelHostTextInput();

}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_HOST_TEXT_INPUT_H_
