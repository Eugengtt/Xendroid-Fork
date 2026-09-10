/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/ui/audio_helper.h"

#include <filesystem>

#include "xenia/base/cvar.h"
#include "xenia/base/logging.h"
#include "xenia/base/string.h"

#define MINIAUDIO_IMPLEMENTATION
#include "third_party/miniaudio/miniaudio.h"

DEFINE_path(achievement_sound_path, "",
            "Path (including filename) to achievement unlock sound. "
            "Supports WAV, MP3, OGG, FLAC, and other common formats.",
            "UI");

namespace xe {
namespace ui {

AudioHelper& AudioHelper::Instance() {
  static AudioHelper instance;
  return instance;
}

AudioHelper::AudioHelper() {}

AudioHelper::~AudioHelper() { Shutdown(); }

void AudioHelper::Initialize() {
  if (initialized_) {
    return;
  }
  initialized_ = true;

  if (cvars::achievement_sound_path.empty()) {
    return;
  }

  std::filesystem::path sound_path = cvars::achievement_sound_path;
  if (!std::filesystem::exists(sound_path)) {
    XELOGE("Achievement sound file not found: {}",
           xe::path_to_utf8(sound_path));
    return;
  }

  // Initialize the audio engine
  engine_ = new ma_engine();
  ma_result result = ma_engine_init(nullptr, engine_);
  if (result != MA_SUCCESS) {
    XELOGE("Failed to initialize miniaudio engine: {}",
           static_cast<int>(result));
    delete engine_;
    engine_ = nullptr;
    return;
  }

  // Load the sound file
  sound_ = new ma_sound();
  std::string sound_path_str = xe::path_to_utf8(sound_path);
  result = ma_sound_init_from_file(engine_, sound_path_str.c_str(), 0, nullptr,
                                   nullptr, sound_);
  if (result != MA_SUCCESS) {
    XELOGE("Failed to load achievement sound: {}", static_cast<int>(result));
    delete sound_;
    sound_ = nullptr;
    return;
  }

  sound_loaded_ = true;
  XELOGI("Achievement sound loaded: {}", sound_path_str);
}

void AudioHelper::Shutdown() {
  if (sound_) {
    ma_sound_uninit(sound_);
    delete sound_;
    sound_ = nullptr;
  }
  if (engine_) {
    ma_engine_uninit(engine_);
    delete engine_;
    engine_ = nullptr;
  }
  sound_loaded_ = false;
  initialized_ = false;
}

void AudioHelper::PlayAchievementSound() {
  // Lazy initialization - only create engine when first needed
  if (!initialized_) {
    Initialize();
  }

  if (sound_loaded_ && sound_) {
    // Stop any currently playing sound and restart from the beginning
    ma_sound_stop(sound_);
    ma_sound_seek_to_pcm_frame(sound_, 0);
    ma_sound_start(sound_);
  }
}

}  // namespace ui
}  // namespace xe
