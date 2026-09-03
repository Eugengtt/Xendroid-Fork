// SPDX-License-Identifier: WTFPL
#include "xenia/kernel/xam/profile_standalone.h"

#include <cstdio>
#include <cstring>
#include <regex>
#include <system_error>

#include "third_party/fmt/include/fmt/format.h"
#include "xenia/base/filesystem.h"
#include "xenia/base/logging.h"
#include "xenia/base/string.h"
#include "xenia/base/string_util.h"
#include "xenia/kernel/title_id_utils.h"
#include "xenia/kernel/xam/profile_manager.h"
#include "xenia/kernel/xam/xam.h"

namespace xe {
namespace kernel {
namespace xam {

namespace {

std::filesystem::path AccountDir(const std::filesystem::path& content_root,
                                 const std::string& xuid_str) {
  return content_root / xuid_str / kDashboardStringID /
         fmt::format("{:08X}", static_cast<uint32_t>(XContentType::kProfile)) /
         xuid_str;
}

bool ReadDecryptAccount(const std::filesystem::path& account_file,
                        X_XAMACCOUNTINFO* out) {
  std::error_code ec;
  const auto size = std::filesystem::file_size(account_file, ec);
  if (ec || size < sizeof(X_XAMACCOUNTINFO) + 0x18) {
    return false;
  }
  FILE* file = xe::filesystem::OpenFile(account_file, "rb");
  if (!file) {
    return false;
  }
  std::vector<uint8_t> data(sizeof(X_XAMACCOUNTINFO) + 0x18);
  const size_t read = fread(data.data(), 1, data.size(), file);
  fclose(file);
  if (read != data.size()) {
    return false;
  }
  if (!ProfileManager::DecryptAccountFile(data.data(), out)) {
    return ProfileManager::DecryptAccountFile(data.data(), out, true);
  }
  return true;
}

// On-disk Account file = 0x10 hash + 8-byte confounder + encrypted
// X_XAMACCOUNTINFO; EncryptAccountFile writes that whole frame.
bool EncryptWriteAccount(const std::filesystem::path& account_file,
                         const X_XAMACCOUNTINFO* account) {
  std::vector<uint8_t> data(sizeof(X_XAMACCOUNTINFO) + 0x18);
  ProfileManager::EncryptAccountFile(account, data.data());

  FILE* file = xe::filesystem::OpenFile(account_file, "wb");
  if (!file) {
    return false;
  }
  const size_t written = fwrite(data.data(), 1, data.size(), file);
  fclose(file);
  return written == data.size();
}

}  // namespace

std::string CreateStandaloneProfile(const std::filesystem::path& content_root,
                                    const std::string& gamertag,
                                    uint32_t language, uint32_t country) {
  if (!ProfileManager::IsGamertagValid(gamertag)) {
    return "";
  }

  const uint64_t xuid = GenerateXuid();
  const std::string xuid_str = fmt::format("{:016X}", xuid);

  std::error_code ec;
  const std::filesystem::path profile_dir = AccountDir(content_root, xuid_str);
  std::filesystem::create_directories(profile_dir, ec);
  if (ec) {
    XELOGE("CreateStandaloneProfile: mkdir failed: {}", ec.message());
    return "";
  }

  X_XAMACCOUNTINFO account = {};
  const std::u16string gamertag_u16 = xe::to_utf16(gamertag);
  string_util::copy_and_swap_truncating(account.gamertag, gamertag_u16,
                                        sizeof(account.gamertag));
  account.SetLanguage(static_cast<XLanguage>(language));
  account.SetCountry(static_cast<XOnlineCountry>(country));

  if (!EncryptWriteAccount(profile_dir / "Account", &account)) {
    XELOGE("CreateStandaloneProfile: failed to write Account for {}", xuid_str);
    return "";
  }
  return xuid_str;
}

std::vector<StandaloneProfile> ListStandaloneProfiles(
    const std::filesystem::path& content_root) {
  std::vector<StandaloneProfile> profiles;

  std::error_code ec;
  if (!std::filesystem::is_directory(content_root, ec)) {
    return profiles;
  }

  const std::regex xuid_pattern("[0-9A-F]{16}");
  const std::string zero_xuid = fmt::format("{:016X}", 0);

  for (std::filesystem::directory_iterator it(content_root, ec), end; it != end;
       it.increment(ec)) {
    if (ec) {
      break;
    }
    if (!it->is_directory(ec)) {
      continue;
    }
    const std::string xuid_str = it->path().filename().string();
    if (!std::regex_match(xuid_str, xuid_pattern) || xuid_str == zero_xuid) {
      continue;
    }

    const std::filesystem::path profile_dir = AccountDir(content_root, xuid_str);
    X_XAMACCOUNTINFO account = {};
    if (!ReadDecryptAccount(profile_dir / "Account", &account)) {
      continue;
    }

    StandaloneProfile profile;
    profile.xuid = xe::string_util::from_string<uint64_t>(xuid_str, true);
    profile.gamertag = account.GetGamertagString();
    profile.language = static_cast<uint32_t>(account.GetLanguage());
    profile.country = static_cast<uint32_t>(account.GetCountry());
    profile.has_avatar =
        std::filesystem::exists(profile_dir / "tile_64.png", ec);
    profiles.push_back(std::move(profile));
  }

  return profiles;
}

X_STATUS RenameStandaloneProfile(const std::filesystem::path& content_root,
                                 uint64_t xuid, const std::string& gamertag,
                                 uint32_t language, uint32_t country) {
  if (!xuid || !ProfileManager::IsGamertagValid(gamertag)) {
    return X_STATUS_INVALID_PARAMETER;
  }

  const std::string xuid_str = fmt::format("{:016X}", xuid);
  const std::filesystem::path account_file =
      AccountDir(content_root, xuid_str) / "Account";

  std::error_code ec;
  if (!std::filesystem::exists(account_file, ec)) {
    return X_STATUS_OBJECT_NAME_NOT_FOUND;
  }

  X_XAMACCOUNTINFO account = {};
  if (!ReadDecryptAccount(account_file, &account)) {
    return X_STATUS_OBJECT_NAME_NOT_FOUND;
  }

  const std::u16string gamertag_u16 = xe::to_utf16(gamertag);
  string_util::copy_and_swap_truncating(account.gamertag, gamertag_u16,
                                        sizeof(account.gamertag));
  account.SetLanguage(static_cast<XLanguage>(language));
  account.SetCountry(static_cast<XOnlineCountry>(country));

  if (!EncryptWriteAccount(account_file, &account)) {
    return X_STATUS_ACCESS_DENIED;
  }
  return X_STATUS_SUCCESS;
}

}  // namespace xam
}  // namespace kernel
}  // namespace xe
