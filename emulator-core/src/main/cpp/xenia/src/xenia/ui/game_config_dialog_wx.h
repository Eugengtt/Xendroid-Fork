/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_GAME_CONFIG_DIALOG_WX_H_
#define XENIA_UI_GAME_CONFIG_DIALOG_WX_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <wx/dialog.h>

class wxBoxSizer;
class wxScrolledWindow;
class wxWindow;

namespace xe {
namespace app {

class EmulatorWindow;

class GameConfigDialog : public wxDialog {
 public:
  GameConfigDialog(wxWindow* parent, EmulatorWindow* emulator_window,
                   uint32_t title_id, std::string game_title);

 private:
  struct Row {
    std::string name;
    wxBoxSizer* sizer = nullptr;
    wxWindow* editor = nullptr;
    std::function<std::string()> get_value;
  };

  void Build();
  void LoadOverrides();
  void SaveOverrides();
  void OnAdd();
  void AddRow(const std::string& name, const std::string& value);
  void RemoveRow(Row* row);

  EmulatorWindow* emulator_window_;
  uint32_t title_id_;
  std::string game_title_;
  bool dirty_ = false;
  wxScrolledWindow* scroll_ = nullptr;
  wxBoxSizer* rows_sizer_ = nullptr;
  std::vector<Row*> rows_;
};

}  // namespace app
}  // namespace xe

#endif  // XENIA_UI_GAME_CONFIG_DIALOG_WX_H_
