/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/ui/imgui_gamepad_dialog.h"

#include "xenia/hid/input_system.h"

namespace xe {
namespace ui {

ImGuiGamepadDialog::ImGuiGamepadDialog(ImGuiDrawer* drawer,
                                       hid::InputSystem* input_system,
                                       bool block_input)
    : ImGuiDialog(drawer), input_system_(input_system) {
  if (input_system_ && block_input) {
    input_system_->AddUIInputBlocker();
    blocking_input_ = true;
  }
}

ImGuiGamepadDialog::~ImGuiGamepadDialog() {
  if (input_system_ && blocking_input_) {
    input_system_->RemoveUIInputBlocker();
  }
}

}  // namespace ui
}  // namespace xe
