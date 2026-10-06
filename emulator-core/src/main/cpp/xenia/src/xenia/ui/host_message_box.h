// SPDX-License-Identifier: WTFPL

#ifndef XENIA_UI_HOST_MESSAGE_BOX_H_
#define XENIA_UI_HOST_MESSAGE_BOX_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace xe {
namespace ui {

struct HostMessageBoxRequest {
  std::string title;
  std::string text;
  std::vector<std::string> buttons;  // never empty
  uint32_t active_button = 0;
  uint32_t flags = 0;
};

struct HostMessageBoxResult {
  uint32_t chosen_button = 0;
};

// Blocks on the calling thread until answered. False = not presentable; caller
// keeps whatever it seeded out_result with.
using HostMessageBoxProvider =
    std::function<bool(const HostMessageBoxRequest&, HostMessageBoxResult&)>;

// Fails every blocked request; the kernel calls this before waiting on the
// dispatch thread, which could otherwise be parked inside a request.
using HostMessageBoxCanceller = std::function<void()>;

void SetHostMessageBoxProvider(HostMessageBoxProvider provider,
                               HostMessageBoxCanceller canceller);
bool HasHostMessageBoxProvider();
bool RequestHostMessageBox(const HostMessageBoxRequest& request,
                           HostMessageBoxResult& out_result);
void CancelHostMessageBox();

}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_HOST_MESSAGE_BOX_H_
