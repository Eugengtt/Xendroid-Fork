/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_PROFILE_EDITOR_DIALOG_WX_H_
#define XENIA_UI_PROFILE_EDITOR_DIALOG_WX_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <wx/dialog.h>

#include "xenia/kernel/xam/user_settings.h"
#include "xenia/kernel/xam/xam.h"
#include "xenia/xbox.h"

class wxButton;
class wxCheckBox;
class wxChoice;
class wxStaticBitmap;
class wxStaticText;
class wxTextCtrl;

namespace xe {
namespace app {

class EmulatorWindow;

class ProfileEditorDialog : public wxDialog {
 public:
  ProfileEditorDialog(wxWindow* parent, EmulatorWindow* emulator_window,
                      uint64_t xuid);

 private:
  struct ProfileData {
    std::string gamertag;
    xe::XOnlineCountry country = xe::XOnlineCountry(0);
    xe::XLanguage language = xe::XLanguage(0);
    bool is_live_enabled = false;
    std::string online_xuid;
    std::string online_domain;
    kernel::xam::X_XAMACCOUNTINFO::AccountSubscriptionTier
        account_subscription_tier =
            kernel::xam::X_XAMACCOUNTINFO::kSubscriptionTierNone;
    std::map<kernel::xam::UserSettingId, kernel::xam::UserDataTypes>
        gpd_settings;
    std::string gamer_name;
    std::string gamer_motto;
    std::string gamer_bio;
    std::vector<uint8_t> profile_icon;
  };

  void LoadProfileData();
  void Build();
  void RefreshIconBitmap();
  void OnChangeIcon();
  void OnSave();
  bool IsGamertagValid() const;
  void UpdateGamertagValidation();

  EmulatorWindow* emulator_window_;
  uint64_t xuid_;
  ProfileData original_data_;
  ProfileData current_data_;

  wxStaticBitmap* icon_bitmap_ = nullptr;
  wxButton* change_icon_button_ = nullptr;
  wxTextCtrl* gamertag_edit_ = nullptr;
  wxStaticText* gamertag_validation_ = nullptr;
  wxTextCtrl* gamer_name_edit_ = nullptr;
  wxTextCtrl* gamer_motto_edit_ = nullptr;
  wxTextCtrl* gamer_bio_edit_ = nullptr;
  wxChoice* language_combo_ = nullptr;
  wxChoice* country_combo_ = nullptr;
  wxCheckBox* live_enabled_check_ = nullptr;
  wxTextCtrl* online_xuid_edit_ = nullptr;
  wxTextCtrl* online_domain_edit_ = nullptr;
  wxChoice* gamer_zone_combo_ = nullptr;
  wxChoice* subscription_tier_combo_ = nullptr;
  wxButton* save_button_ = nullptr;
};

}  // namespace app
}  // namespace xe

#endif  // XENIA_UI_PROFILE_EDITOR_DIALOG_WX_H_
