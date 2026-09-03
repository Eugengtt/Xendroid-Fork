/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_IMGUI_CONTEXT_MENU_H_
#define XENIA_UI_IMGUI_CONTEXT_MENU_H_

#include <functional>
#include <string>
#include <vector>

#include "xenia/ui/imgui_dialog.h"

namespace xe {
namespace hid {
class InputSystem;
}  // namespace hid
}  // namespace xe

namespace xe {
namespace ui {

// ImGui-based context menu with gamepad support.
// This is a modal popup that blocks game input while open.
class ImGuiContextMenu : public ImGuiDialog {
 public:
  struct MenuItem {
    std::string text;
    std::string shortcut;
    std::function<void()> callback;
    bool is_separator;
  };

  // Creates a context menu. If input_system is provided, gamepad input
  // will be supported and game input will be blocked while menu is open.
  ImGuiContextMenu(ImGuiDrawer* drawer, hid::InputSystem* input_system);
  ~ImGuiContextMenu() override;

  // Add a menu item with optional keyboard shortcut hint
  void AddAction(const std::string& text, std::function<void()> callback,
                 const std::string& shortcut = "");

  // Add a visual separator line
  void AddSeparator();

  // Show the menu centered on screen
  void Show();

  // Show the menu at a specific position (in screen coordinates)
  void ShowAt(float x, float y);

  // Close the menu programmatically
  void CloseMenu() { Close(); }

  // Set a callback to be invoked when the menu closes
  void SetOnCloseCallback(std::function<void()> callback) {
    on_close_callback_ = std::move(callback);
  }

 protected:
  void OnClose() override;
  void OnDraw(ImGuiIO& io) override;

 private:
  void PollGamepad();
  void ActivateItem(int index);
  int GetNextSelectableItem(int current, int direction);

  std::vector<MenuItem> items_;
  hid::InputSystem* input_system_;
  std::function<void()> on_close_callback_;
  std::function<void()> pending_callback_;  // Callback to execute after close
  int focused_index_ = 0;
  uint16_t prev_buttons_ = 0;
  bool has_opened_ = false;
  bool activation_pending_ = false;
  bool center_on_screen_ = true;
  float position_x_ = 0;
  float position_y_ = 0;
};

}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_IMGUI_CONTEXT_MENU_H_
