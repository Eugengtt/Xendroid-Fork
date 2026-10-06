/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_AUDIO_HELPER_H_
#define XENIA_UI_AUDIO_HELPER_H_

// Forward declare miniaudio types to avoid header pollution
struct ma_engine;
struct ma_sound;

namespace xe {
namespace ui {

// Singleton audio helper for playing notification sounds.
// Uses miniaudio for cross-platform audio playback.
class AudioHelper {
 public:
  static AudioHelper& Instance();

  // Play the achievement unlock sound if configured
  void PlayAchievementSound();

 private:
  AudioHelper();
  ~AudioHelper();

  // Non-copyable
  AudioHelper(const AudioHelper&) = delete;
  AudioHelper& operator=(const AudioHelper&) = delete;

  void Initialize();
  void Shutdown();

  ma_engine* engine_ = nullptr;
  ma_sound* sound_ = nullptr;
  bool initialized_ = false;
  bool sound_loaded_ = false;
};

}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_AUDIO_HELPER_H_
