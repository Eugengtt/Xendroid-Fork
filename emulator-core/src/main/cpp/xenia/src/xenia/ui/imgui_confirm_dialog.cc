/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/ui/imgui_confirm_dialog.h"

#include <cfloat>
#include <sstream>

#include "third_party/imgui/imgui.h"
#include "xenia/hid/input_system.h"

namespace xe {
namespace ui {

ImGuiConfirmDialog::ImGuiConfirmDialog(ImGuiDrawer* drawer,
                                       const std::string& title,
                                       const std::string& message,
                                       Callback callback,
                                       hid::InputSystem* input_system)
    : ImGuiDialog(drawer),
      title_(title),
      message_(message),
      callback_(std::move(callback)),
      input_system_(input_system) {
  if (input_system_) {
    input_system_->AddUIInputBlocker();
  }
}

ImGuiConfirmDialog::~ImGuiConfirmDialog() {
  if (input_system_) {
    input_system_->RemoveUIInputBlocker();
  }
}

void ImGuiConfirmDialog::Confirm(bool result) {
  if (!callback_invoked_) {
    callback_invoked_ = true;
    if (callback_) {
      callback_(result);
    }
    Close();
  }
}

void ImGuiConfirmDialog::OnDraw(ImGuiIO& io) {
  // Open popup on first draw
  if (!has_opened_) {
    ImGui::OpenPopup(title_.c_str());
    has_opened_ = true;
  }

  // Style the popup - white background, black text, Xbox green accents
  const ImVec4 xbox_green(0.063f, 0.486f, 0.063f, 1.0f);
  ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_TitleBg, xbox_green);
  ImGui::PushStyleColor(ImGuiCol_TitleBgActive, xbox_green);
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

  // Center the popup on screen
  ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

  // Set minimum width based on font size
  float min_width = ImGui::GetFontSize() * 20.0f;
  ImGui::SetNextWindowSizeConstraints(ImVec2(min_width, 0),
                                      ImVec2(FLT_MAX, FLT_MAX));

  bool is_open = true;
  if (ImGui::BeginPopupModal(title_.c_str(), &is_open,
                             ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoScrollbar)) {
    // Handle keyboard escape or gamepad B/Back to cancel
    if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ShouldCloseFromGamepad()) {
      Confirm(false);
    }

    // Message text - center each line
    float window_width = ImGui::GetWindowWidth();
    std::istringstream stream(message_);
    std::string line;
    while (std::getline(stream, line)) {
      float text_width = ImGui::CalcTextSize(line.c_str()).x;
      ImGui::SetCursorPosX((window_width - text_width) * 0.5f);
      ImGui::TextUnformatted(line.c_str());
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Buttons - centered, using scaled sizes
    const auto& style = ImGui::GetStyle();
    float frame_height = ImGui::GetFrameHeight();
    float button_width = frame_height * 4.0f;
    float spacing = style.ItemSpacing.x;
    float total_width = button_width * 2 + spacing;
    float start_x = (ImGui::GetWindowWidth() - total_width) * 0.5f;

    ImGui::SetCursorPosX(start_x);

    // Style for buttons - light gray with Xbox green hover
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, xbox_green);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, xbox_green);

    // No button
    if (ImGui::Button("No", ImVec2(button_width, 0))) {
      Confirm(false);
    }

    ImGui::SameLine(0, spacing);

    // Yes button - set as default focus
    if (ImGui::Button("Yes", ImVec2(button_width, 0))) {
      Confirm(true);
    }
    ImGui::SetItemDefaultFocus();

    ImGui::PopStyleColor(3);

    ImGui::EndPopup();
  } else {
    // Popup was closed (clicked outside or X button)
    Confirm(false);
  }

  ImGui::PopStyleVar(1);
  ImGui::PopStyleColor(5);
}

}  // namespace ui
}  // namespace xe
