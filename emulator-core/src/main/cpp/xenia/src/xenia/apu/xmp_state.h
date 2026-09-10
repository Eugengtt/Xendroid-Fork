/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_APU_XMP_STATE_H_
#define XENIA_APU_XMP_STATE_H_

#include <cstdint>

namespace xe {
namespace apu {

// Wire ABI values from the Xbox 360 XMP API. Values must not be changed.
enum class XmpClient : uint32_t {
  kDash = 0,
  kHud = 1,
  kGame = 2,
  kRemote = 3,
  kMusicPlayer = 4,
  kMsal = 5,
  kMce = 6,
};

// Wire ABI values from the Xbox 360 XMP API. Values must not be changed.
enum class PlaybackController : uint32_t {
  kGame = 0,
  kUser = 1,
  kDash = 2,
  kMce = 3,
  kRestore = 4,
};

}  // namespace apu
}  // namespace xe

#endif  // XENIA_APU_XMP_STATE_H_
