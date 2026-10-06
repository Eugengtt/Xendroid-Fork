/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_IMGUI_GAMEPAD_DIALOG_H_
#define XENIA_UI_IMGUI_GAMEPAD_DIALOG_H_

#include "xenia/ui/imgui_dialog.h"

namespace xe {
namespace hid {
class InputSystem;
}  // namespace hid
}  // namespace xe

namespace xe {
namespace ui {

// Base class for ImGui dialogs that need to block game input.
// Gamepad navigation is handled by ImGui's native system.
// Dialogs should check ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight) for
// B-to-close.
class ImGuiGamepadDialog : public ImGuiDialog {
 public:
  // When block_input is true, game input is blocked while dialog is open
  ImGuiGamepadDialog(ImGuiDrawer* drawer, hid::InputSystem* input_system,
                     bool block_input = true);
  ~ImGuiGamepadDialog() override;

 protected:
  hid::InputSystem* input_system_;
  bool blocking_input_ = false;
};

}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_IMGUI_GAMEPAD_DIALOG_H_
