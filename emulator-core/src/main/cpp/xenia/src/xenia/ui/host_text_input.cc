// SPDX-License-Identifier: WTFPL

#include "xenia/ui/host_text_input.h"

#include <mutex>

namespace xe {
namespace ui {

namespace {

struct ProviderRegistry {
  std::mutex mutex;
  HostTextInputProvider provider;
  HostTextInputCanceller canceller;
};

ProviderRegistry& registry() {
  static ProviderRegistry registry;
  return registry;
}

}  // namespace

void SetHostTextInputProvider(HostTextInputProvider provider,
                              HostTextInputCanceller canceller) {
  auto& reg = registry();
  std::lock_guard<std::mutex> lock(reg.mutex);
  reg.provider = std::move(provider);
  reg.canceller = std::move(canceller);
}

bool HasHostTextInputProvider() {
  auto& reg = registry();
  std::lock_guard<std::mutex> lock(reg.mutex);
  return static_cast<bool>(reg.provider);
}

bool RequestHostTextInput(const HostTextInputRequest& request,
                          HostTextInputResult& out_result) {
  HostTextInputProvider provider;
  {
    auto& reg = registry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    provider = reg.provider;
  }
  // Invoked unlocked: this blocks for as long as the user takes to type.
  return provider ? provider(request, out_result) : false;
}

void CancelHostTextInput() {
  HostTextInputCanceller canceller;
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
