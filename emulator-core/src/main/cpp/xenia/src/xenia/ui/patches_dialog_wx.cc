/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/ui/patches_dialog_wx.h"

#include <wx/checkbox.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "third_party/fmt/include/fmt/format.h"

#include "xenia/app/emulator_window.h"
#include "xenia/base/filesystem.h"
#include "xenia/emulator.h"

namespace xe {
namespace app {

PatchesDialog::PatchesDialog(wxWindow* parent, EmulatorWindow* emulator_window,
                             uint32_t title_id,
                             patcher::BundledPatchFile bundled)
    : wxDialog(
          parent, wxID_ANY,
          wxString::Format(_("%s (%08X)"),
                           wxString::FromUTF8(bundled.entry.title_name.empty()
                                                  ? bundled.filename
                                                  : bundled.entry.title_name),
                           title_id),
          wxDefaultPosition, wxSize(640, 480),
          wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      emulator_window_(emulator_window),
      title_id_(title_id) {
  std::filesystem::path storage_path;
  if (emulator_window_ && emulator_window_->emulator()) {
    storage_path = emulator_window_->emulator()->storage_root() / "patches" /
                   bundled.filename;
  }

  std::string source_text;
  if (!storage_path.empty()) {
    source_text = xe::filesystem::ReadAllText(storage_path);
  }
  if (source_text.empty()) {
    source_text = std::move(bundled.toml_content);
  }

  editor_ =
      std::make_unique<patcher::PatchFileEditor>(source_text, storage_path);

  Build();
}

void PatchesDialog::Build() {
  auto* sizer = new wxBoxSizer(wxVERTICAL);

  auto* scroll = new wxScrolledWindow(this, wxID_ANY);
  scroll->SetScrollRate(0, 16);
  scroll_ = scroll;
  auto* scroll_sizer = new wxBoxSizer(wxVERTICAL);

  const auto& patches = editor_->patches();
  for (size_t i = 0; i < patches.size(); ++i) {
    const auto& info = patches[i];
    wxString display_name;
    if (info.name.empty()) {
      display_name = wxString::Format(_("Patch #%zu"), i + 1);
    } else {
      display_name = wxString::FromUTF8(info.name);
    }

    auto* checkbox = new wxCheckBox(scroll, wxID_ANY, display_name);
    checkbox->SetValue(info.is_enabled);
    checkbox->Bind(wxEVT_CHECKBOX,
                   [this, idx = i, cb = checkbox](wxCommandEvent&) {
                     OnToggle(idx, cb->GetValue());
                   });
    scroll_sizer->Add(checkbox, wxSizerFlags().Border(wxLEFT | wxTOP, 8));

    if (!info.description.empty() || !info.author.empty()) {
      wxString detail;
      if (!info.description.empty()) {
        detail = wxString::FromUTF8(info.description);
      }
      if (!info.author.empty()) {
        if (!detail.empty()) {
          detail += wxT("\n");
        }
        detail += wxString::Format(_("by %s"), wxString::FromUTF8(info.author));
      }
      auto* lbl = new wxStaticText(scroll, wxID_ANY, detail);
      lbl->SetForegroundColour(*wxLIGHT_GREY);
      scroll_sizer->Add(lbl,
                        wxSizerFlags().Border(wxLEFT | wxRIGHT, 32).Expand());
      desc_labels_.emplace_back(lbl, detail);
    }
  }

  scroll->SetSizer(scroll_sizer);
  scroll->Bind(wxEVT_SIZE, &PatchesDialog::OnScrollSize, this);
  sizer->Add(scroll, wxSizerFlags(1).Expand().Border(wxALL, 8));

  info_label_ = new wxStaticText(this, wxID_ANY,
                                 _("Toggles take effect on next launch."));
  sizer->Add(info_label_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 12));

  auto* button_sizer = CreateButtonSizer(wxCLOSE);
  if (button_sizer) {
    sizer->Add(button_sizer, wxSizerFlags().Right().Border(wxALL, 8));
  }

  SetSizer(sizer);
}

void PatchesDialog::OnToggle(size_t patch_index, bool new_value) {
  if (!editor_->SetEnabled(patch_index, new_value)) {
    if (info_label_) {
      info_label_->SetLabel(_("Failed to save changes."));
    }
    return;
  }
  if (info_label_) {
    info_label_->SetLabel(_("Saved. Takes effect on next launch."));
  }
}

void PatchesDialog::OnScrollSize(wxSizeEvent& event) {
  event.Skip();
  RewrapDescriptions();
}

void PatchesDialog::RewrapDescriptions() {
  if (!scroll_ || desc_labels_.empty()) {
    return;
  }
  // 32px left + 32px right border on each label inside the scroll viewport.
  int width = scroll_->GetClientSize().GetWidth() - 64;
  if (width <= 0) {
    return;
  }
  if (width == last_wrap_width_) {
    return;
  }
  last_wrap_width_ = width;
  for (auto& [label, text] : desc_labels_) {
    label->SetLabel(text);
    label->Wrap(width);
  }
  if (auto* s = scroll_->GetSizer()) {
    s->Layout();
  }
  scroll_->FitInside();
}

}  // namespace app
}  // namespace xe
