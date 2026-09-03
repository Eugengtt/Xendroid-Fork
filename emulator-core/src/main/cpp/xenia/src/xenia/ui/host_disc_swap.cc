// SPDX-License-Identifier: WTFPL

#include "xenia/ui/host_disc_swap.h"

#include <mutex>

namespace xe {
namespace ui {

namespace {

struct ProviderRegistry {
  std::mutex mutex;
  HostDiscSwapProvider provider;
  HostDiscSwapCanceller canceller;
};

ProviderRegistry& registry() {
  static ProviderRegistry registry;
  return registry;
}

}  // namespace

void SetHostDiscSwapProvider(HostDiscSwapProvider provider,
                             HostDiscSwapCanceller canceller) {
  auto& reg = registry();
  std::lock_guard<std::mutex> lock(reg.mutex);
  reg.provider = std::move(provider);
  reg.canceller = std::move(canceller);
}

bool HasHostDiscSwapProvider() {
  auto& reg = registry();
  std::lock_guard<std::mutex> lock(reg.mutex);
  return static_cast<bool>(reg.provider);
}

bool RequestHostDiscSwap(const HostDiscSwapRequest& request,
                         HostDiscSwapResult& out_result) {
  HostDiscSwapProvider provider;
  {
    auto& reg = registry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    provider = reg.provider;
  }
  // Invoked unlocked: this blocks for as long as the user takes to choose.
  return provider ? provider(request, out_result) : false;
}

void CancelHostDiscSwap() {
  HostDiscSwapCanceller canceller;
  {
    auto& reg = registry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    canceller = reg.canceller;
  }
  if (canceller) {
    canceller();
  }
}

}  // namespace ui
}  // namespace xe
