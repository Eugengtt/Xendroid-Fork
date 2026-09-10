/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_QUICK_SETTINGS_DIALOG_WX_H_
#define XENIA_UI_QUICK_SETTINGS_DIALOG_WX_H_

#include <map>
#include <string>

#include <wx/dialog.h>

class wxCheckBox;
class wxChoice;
class wxRadioButton;
class wxSpinCtrl;
class wxStaticText;
class wxWindow;

namespace xe {
namespace app {

class EmulatorWindow;

class QuickSettingsDialog : public wxDialog {
 public:
  // ShowModal returns this when the user clicked "Advanced…" so the caller
  // can open the advanced dialog after this one is destroyed.
  static constexpr int kReturnAdvancedRequested = wxID_HIGHEST + 1;

  QuickSettingsDialog(wxWindow* parent, EmulatorWindow* emulator_window);

 private:
  struct Option {
    std::string cvar_name;
    std::string current_value;
    std::string pending_value;
    wxWindow* editor = nullptr;
    wxStaticText* label = nullptr;
  };

  void Build();
  void LoadValuesFromCvars();
  void Save();
  void ResetToDefaults();
  void OnAdvanced();
  void OnAnyChanged();
  std::string ReadEditor(const Option& option);
  void UpdateLabelBold(Option& option);

  EmulatorWindow* emulator_window_;
  std::map<std::string, Option> options_;
  bool has_unsaved_changes_ = false;

  // Refresh-rate radios stored separately because the cvar maps to two cvars.
  wxRadioButton* refresh_uncapped_ = nullptr;
  wxRadioButton* refresh_50hz_ = nullptr;
  wxRadioButton* refresh_60hz_ = nullptr;
};

}  // namespace app
}  // namespace xe

#endif  // XENIA_UI_QUICK_SETTINGS_DIALOG_WX_H_
