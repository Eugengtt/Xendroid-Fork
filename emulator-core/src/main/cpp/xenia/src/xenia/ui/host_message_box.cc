// SPDX-License-Identifier: WTFPL

#include "xenia/ui/host_message_box.h"

#include <mutex>

namespace xe {
namespace ui {

namespace {

struct ProviderRegistry {
  std::mutex mutex;
  HostMessageBoxProvider provider;
  HostMessageBoxCanceller canceller;
};

ProviderRegistry& registry() {
  static ProviderRegistry registry;
  return registry;
}

}  // namespace

void SetHostMessageBoxProvider(HostMessageBoxProvider provider,
                               HostMessageBoxCanceller canceller) {
  auto& reg = registry();
  std::lock_guard<std::mutex> lock(reg.mutex);
  reg.provider = std::move(provider);
  reg.canceller = std::move(canceller);
}

bool HasHostMessageBoxProvider() {
  auto& reg = registry();
  std::lock_guard<std::mutex> lock(reg.mutex);
  return static_cast<bool>(reg.provider);
}

bool RequestHostMessageBox(const HostMessageBoxRequest& request,
                           HostMessageBoxResult& out_result) {
  HostMessageBoxProvider provider;
  {
    auto& reg = registry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    provider = reg.provider;
  }
  // Invoked unlocked: blocks until the user answers.
  return provider ? provider(request, out_result) : false;
}

void CancelHostMessageBox() {
  HostMessageBoxCanceller canceller;
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
