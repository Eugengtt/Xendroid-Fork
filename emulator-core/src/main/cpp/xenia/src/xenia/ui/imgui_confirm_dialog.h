/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_IMGUI_CONFIRM_DIALOG_H_
#define XENIA_UI_IMGUI_CONFIRM_DIALOG_H_

#include <functional>
#include <string>

#include "xenia/ui/imgui_dialog.h"

namespace xe {
namespace hid {
class InputSystem;
}  // namespace hid
}  // namespace xe

namespace xe {
namespace ui {

// ImGui-based confirmation dialog with gamepad support.
// Uses async callback pattern - the callback is invoked when the user
// confirms or cancels the dialog.
class ImGuiConfirmDialog : public ImGuiDialog {
 public:
  using Callback = std::function<void(bool confirmed)>;

  // Creates a confirm dialog. The callback will be invoked with true if
  // the user confirms, false if they cancel.
  // If input_system is provided, gamepad input will be supported.
  ImGuiConfirmDialog(ImGuiDrawer* drawer, const std::string& title,
                     const std::string& message, Callback callback,
                     hid::InputSystem* input_system = nullptr);

  ~ImGuiConfirmDialog() override;

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  void Confirm(bool result);

  std::string title_;
  std::string message_;
  Callback callback_;
  hid::InputSystem* input_system_;
  bool has_opened_ = false;
  bool callback_invoked_ = false;
};

}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_IMGUI_CONFIRM_DIALOG_H_
