// SPDX-License-Identifier: WTFPL

#ifndef XENIA_UI_HOST_DISC_SWAP_H_
#define XENIA_UI_HOST_DISC_SWAP_H_

#include <functional>
#include <string>
#include <vector>

namespace xe {
namespace ui {

// A host-rendered replacement for the ImGui disc-swap dialog. No provider is
// installed by default.

struct HostDiscSwapDisc {
  std::string label;
  std::string path_utf8;  // absolute host path, handed back verbatim
};

struct HostDiscSwapRequest {
  std::string message;
  bool is_error = false;
  uint32_t disc_number = 0;  // 1-based
  // Empty means the provider must offer its own way to choose.
  std::vector<HostDiscSwapDisc> discs;
};

struct HostDiscSwapResult {
  bool accepted = false;
  std::string path_utf8;
};

// Blocks the guest thread until answered. False means it could not be
// presented; treat as a cancellation.
using HostDiscSwapProvider =
    std::function<bool(const HostDiscSwapRequest&, HostDiscSwapResult&)>;

// Fails every blocked request, for teardown with a guest thread parked in one.
using HostDiscSwapCanceller = std::function<void()>;

void SetHostDiscSwapProvider(HostDiscSwapProvider provider,
                             HostDiscSwapCanceller canceller);
bool HasHostDiscSwapProvider();
bool RequestHostDiscSwap(const HostDiscSwapRequest& request,
                         HostDiscSwapResult& out_result);
void CancelHostDiscSwap();

}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_HOST_DISC_SWAP_H_
