/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/helper/sdl/sdl_helper.h"

#include "xenia/base/assert.h"
#include "xenia/base/logging.h"

namespace xe {
namespace helper {
namespace sdl {
bool SDLHelper::is_prepared_ = false;

bool SDLHelper::Prepare() {
  if (is_prepared_) {
    return true;
  }
  is_prepared_ = true;

  is_prepared_ &= SetHints();
  is_prepared_ &= RedirectLog();

  const int ver = SDL_GetVersion();
  XELOGI("SDL Version {}.{}.{} initialized.", SDL_VERSIONNUM_MAJOR(ver),
         SDL_VERSIONNUM_MINOR(ver), SDL_VERSIONNUM_MICRO(ver));

  return is_prepared_;
}

bool SDLHelper::SetHints() {
  bool suc = true;

  const auto setHint = [](const char* name, const char* value,
                          bool override_ = false) -> bool {
    // Setting hints with normal priority fails when the hint is set via env
    // vars or a hint with override priority is set, which does not conclude a
    // failure.
    if (!SDL_SetHintWithPriority(
            name, value, override_ ? SDL_HINT_OVERRIDE : SDL_HINT_NORMAL)) {
      const char* msg_fmt =
          "SDLHelper: Unable to set hint \"{}\" to value \"{}\".";
      if (override_) {
        XELOGE(msg_fmt, name, value);
        return false;
      } else {
        XELOGI(msg_fmt, name, value);
      }
    }
    return true;
  };

  suc &= setHint(SDL_HINT_AUDIO_CATEGORY, "playback");

  // SDL3 replaced SDL_HINT_AUDIO_DEVICE_APP_NAME with app metadata. Backends
  // (PulseAudio, PipeWire, WASAPI session names, etc.) read this for the
  // user-facing device label.
  SDL_SetAppMetadata("xenia emulator", nullptr, nullptr);

  // On Windows, SDL's joystick subsystem creates a hidden device-notification
  // window (WM_DEVICECHANGE + raw-input dispatch) on whichever thread inits
  // it. Without this hint that window lives on our SDL worker thread, which
  // doesn't pump Win32 messages — leaving the queue and STA apartment state
  // in a broken state that destabilises the UI thread via cross-apartment
  // marshaling. With it, SDL spawns its own thread that owns and pumps the
  // window. No-op on non-Windows platforms.
  suc &= setHint(SDL_HINT_JOYSTICK_THREAD, "1", true);

  // Force SDL's XInput backend over RAWINPUT: it bakes the XInput SubType
  // into the joystick name (e.g. "XInput Guitar #1"), letting us classify
  // form factor by name.
  suc &= setHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");

  return suc;
}

bool SDLHelper::RedirectLog() {
  // Redirect SDL_Log* output (internal library stuff) to our log system.
  SDL_SetLogOutputFunction(
      [](void* userdata, int category, SDL_LogPriority priority,
         const char* message) {
        const char* msg_fmt = "SDL: {}";
        switch (priority) {
          case SDL_LOG_PRIORITY_VERBOSE:
          case SDL_LOG_PRIORITY_DEBUG:
            XELOGD(msg_fmt, message);
            break;
          case SDL_LOG_PRIORITY_INFO:
            XELOGI(msg_fmt, message);
            break;
          case SDL_LOG_PRIORITY_WARN:
            XELOGW(msg_fmt, message);
            break;
          case SDL_LOG_PRIORITY_ERROR:
          case SDL_LOG_PRIORITY_CRITICAL:
            XELOGE(msg_fmt, message);
            break;
          default:
            XELOGI(msg_fmt, message);
            assert_always("SDL: Unknown log priority");
            break;
        }
      },
      nullptr);
  // SDL itself isn't that talkative. Additionally to this settings there are
  // hints that can be switched on to output additional internal logging
  // information.
  SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
  return true;
}
}  // namespace sdl
}  // namespace helper
}  // namespace xe
