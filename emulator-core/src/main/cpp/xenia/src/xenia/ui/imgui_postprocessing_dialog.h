/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_IMGUI_POSTPROCESSING_DIALOG_H_
#define XENIA_UI_IMGUI_POSTPROCESSING_DIALOG_H_

#include <functional>
#include <string>

#include "xenia/ui/imgui_gamepad_dialog.h"

namespace xe {
namespace app {
class EmulatorWindow;
}  // namespace app
}  // namespace xe

namespace xe {
namespace ui {

// ImGui-based post-processing settings dialog.
// Based on the original DisplayConfigDialog from upstream.
class ImGuiPostProcessingDialog : public ImGuiGamepadDialog {
 public:
  ImGuiPostProcessingDialog(ImGuiDrawer* drawer,
                            app::EmulatorWindow* emulator_window,
                            hid::InputSystem* input_system);

  void CloseDialog() { Close(); }

  void SetOnCloseCallback(std::function<void()> callback) {
    on_close_callback_ = std::move(callback);
  }

 protected:
  void OnClose() override;
  void OnDraw(ImGuiIO& io) override;

 private:
  void LoadCurrentSettings();
  void ShowNotification(const std::string& title,
                        const std::string& description);

  // Setting change handlers
  void OnAntiAliasingChanged(int value);
  void OnResamplingChanged(int value);
  void OnFsrSharpnessChanged(float value);
  void OnFsrMaxUpsamplingPassesChanged(int value);
  void OnCasSharpnessChanged(float value);
  void OnDitherChanged(bool value);

  app::EmulatorWindow* emulator_window_;
  std::function<void()> on_close_callback_;

  // Current settings state
  int anti_aliasing_mode_ = 0;  // 0=None, 1=FXAA, 2=FXAA Extreme
  int resampling_mode_ = 0;     // 0=Bilinear, 1=CAS, 2=FSR
  float fsr_sharpness_ = 0.0f;
  int fsr_max_upsampling_passes_ = 5;
  float cas_additional_sharpness_ = 0.0f;
  bool dither_ = false;

  // Highlight positions for navigation
  int aa_highlight_ = 0;
  int resampling_highlight_ = 0;
};

}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_IMGUI_POSTPROCESSING_DIALOG_H_
