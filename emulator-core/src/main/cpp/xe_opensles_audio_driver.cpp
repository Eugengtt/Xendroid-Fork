/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xe_opensles_audio_driver.h"
#include "xenia/base/logging.h"
#include "xenia/base/assert.h"
#include "xenia/apu/conversion.h"
#include "xenia/apu/apu_flags.h"
#include "xenia/base/profiling.h"

namespace xe {
namespace apu {
namespace opensles {

OpenSLESAudioDriver::OpenSLESAudioDriver(Memory* memory, xe::threading::Semaphore* semaphore)
    : semaphore_(semaphore) {}

OpenSLESAudioDriver::~OpenSLESAudioDriver() {
}

bool OpenSLESAudioDriver::Initialize() {
    SLresult r;

    r = slCreateEngine(&sl_object_, 0, nullptr, 0, nullptr, nullptr);
    if (r != SL_RESULT_SUCCESS) {
        XELOGE("slCreateEngine failed: {}", r);
        return false;
    }

    r = (*sl_object_)->Realize(sl_object_, SL_BOOLEAN_FALSE);
    if (r != SL_RESULT_SUCCESS) {
        XELOGE("Realize engine failed: {}", r);
        return false;
    }

    r = (*sl_object_)->GetInterface(sl_object_, SL_IID_ENGINE, &sl_engine_);
    if (r != SL_RESULT_SUCCESS) {
        XELOGE("GetInterface engine failed: {}", r);
        return false;
    }

    const SLInterfaceID ids[] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean req[] = {SL_BOOLEAN_FALSE};
    r = (*sl_engine_)->CreateOutputMix(sl_engine_, &sl_output_mix_, 1, ids, req);
    if (r != SL_RESULT_SUCCESS) {
        XELOGE("CreateOutputMix failed: {}", r);
        return false;
    }

    r = (*sl_output_mix_)->Realize(sl_output_mix_, SL_BOOLEAN_FALSE);
    if (r != SL_RESULT_SUCCESS) {
        XELOGE("Realize output mix failed: {}", r);
        return false;
    }

    SLDataLocator_AndroidSimpleBufferQueue locator_bufferqueue = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};

    SLAndroidDataFormat_PCM_EX format_pcm = {
        SL_ANDROID_DATAFORMAT_PCM_EX,
        2,
        SL_SAMPLINGRATE_48,
        SL_PCMSAMPLEFORMAT_FIXED_32,
        SL_PCMSAMPLEFORMAT_FIXED_32,
        SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
        SL_BYTEORDER_LITTLEENDIAN,
        SL_ANDROID_PCM_REPRESENTATION_FLOAT
    };

    SLDataSource audioSrc = {&locator_bufferqueue, &format_pcm};

    SLDataLocator_OutputMix locator_outputmix = {SL_DATALOCATOR_OUTPUTMIX, sl_output_mix_};
    SLDataSink audioSnk = {&locator_outputmix, NULL};

    const SLInterfaceID interfaceIds[] = {
        SL_IID_BUFFERQUEUE,
        SL_IID_EFFECTSEND,
        SL_IID_VOLUME
    };

    const SLboolean interfaceRequired[] = {
        SL_BOOLEAN_TRUE,
        SL_BOOLEAN_TRUE,
        SL_BOOLEAN_TRUE
    };

    r = (*sl_engine_)->CreateAudioPlayer(
        sl_engine_,
        &sl_player_,
        &audioSrc,
        &audioSnk,
        sizeof(interfaceIds)/sizeof(interfaceIds[0]),
        interfaceIds,
        interfaceRequired
    );

    if (r != SL_RESULT_SUCCESS) {
        XELOGE("CreateAudioPlayer failed: {}", r);
        return false;
    }

    r = (*sl_player_)->Realize(sl_player_, SL_BOOLEAN_FALSE);
    if (r != SL_RESULT_SUCCESS) {
        XELOGE("Realize player failed: {}", r);
        return false;
    }

    r = (*sl_player_)->GetInterface(sl_player_, SL_IID_PLAY, &sl_player_play_);
    if (r != SL_RESULT_SUCCESS) {
        XELOGE("GetInterface play failed: {}", r);
        return false;
    }

    r = (*sl_player_)->GetInterface(sl_player_, SL_IID_BUFFERQUEUE, &sl_player_buffer_queue_);
    if (r != SL_RESULT_SUCCESS) {
        XELOGE("GetInterface buffer queue failed: {}", r);
        return false;
    }

    r = (*sl_player_)->GetInterface(sl_player_, SL_IID_EFFECTSEND, &sl_player_effect_send_);
    if (r != SL_RESULT_SUCCESS) {
        XELOGE("GetInterface effect send failed: {}", r);
        return false;
    }

    r = (*sl_player_)->GetInterface(sl_player_, SL_IID_VOLUME, &sl_player_volume_);
    if (r != SL_RESULT_SUCCESS) {
        XELOGE("GetInterface volume failed: {}", r);
        return false;
    }

    {
        std::unique_lock<std::mutex> guard(frames_mutex_);
        for (int i = 0; i < 2; i++) {
            float* buffer = new float[x360_frame_channels_ * channel_samples_];
            frames_unused_.push(buffer);
        }
    }

    r = (*sl_player_buffer_queue_)->RegisterCallback(sl_player_buffer_queue_, PlayerCallback, this);
    if (r != SL_RESULT_SUCCESS) {
        XELOGE("RegisterCallback failed: {}", r);
        return false;
    }

    r = (*sl_player_play_)->SetPlayState(sl_player_play_, SL_PLAYSTATE_PLAYING);
    if (r != SL_RESULT_SUCCESS) {
        XELOGE("SetPlayState failed: {}", r);
        return false;
    }

    PlayerCallback(sl_player_buffer_queue_, this);

    return true;
}

void OpenSLESAudioDriver::Pause() {
    SLresult r = (*sl_player_play_)->SetPlayState(sl_player_play_, SL_PLAYSTATE_STOPPED);
    if (r != SL_RESULT_SUCCESS) {
        XELOGE("SetPlayState failed: {}", r);
    }
}

void OpenSLESAudioDriver::Resume() {
    SLresult r = (*sl_player_play_)->SetPlayState(sl_player_play_, SL_PLAYSTATE_PLAYING);
    if (r != SL_RESULT_SUCCESS) {
        XELOGE("SetPlayState failed: {}", r);
    }
}

void OpenSLESAudioDriver::SetVolume(float volume) {
    SLresult r = (*sl_player_volume_)->SetVolumeLevel(sl_player_volume_, static_cast<SLmillibel>(volume * 100.0f));
    if (r != SL_RESULT_SUCCESS) {
        XELOGE("SetVolumeLevel failed: {}", r);
    }
}

void OpenSLESAudioDriver::PlayerCallback(SLAndroidSimpleBufferQueueItf buffer_queue, void* context) {
  SCOPE_profile_cpu_f("apu");

  auto driver = static_cast<OpenSLESAudioDriver*>(context);
  std::unique_lock<std::mutex> guard(driver->frames_mutex_);

  constexpr size_t output_frame_size=OpenSLESAudioDriver::host_frame_channels_ * OpenSLESAudioDriver::channel_samples_;
  static float output_frame[output_frame_size]={0};

  if (driver->frames_queued_.empty()) {
    std::memset(output_frame, 0, sizeof(output_frame));
    (*buffer_queue)->Enqueue(buffer_queue, output_frame, sizeof(output_frame));
    return;
  } else {
    auto buffer = driver->frames_queued_.front();
    driver->frames_queued_.pop();

    if (cvars::volume == 0) {  // XenDroid: edge replaced cvars::mute with volume (0 == mute)
      std::memset(output_frame, 0, sizeof(output_frame));
    } else {
      conversion::sequential_6_BE_to_interleaved_2_LE(
          output_frame, buffer, driver->channel_samples_);
    }

    driver->frames_unused_.push(buffer);
  }

  (*buffer_queue)->Enqueue(buffer_queue, output_frame, sizeof(output_frame));

  auto ret = driver->semaphore_->Release(1, nullptr);
  assert_true(ret);
}

void OpenSLESAudioDriver::SubmitFrame(float* samples) {
    const auto input_frame = samples;
  constexpr auto x360_frame_samples = x360_frame_channels_ * channel_samples_;
  float* output_frame;

  {
    std::unique_lock<std::mutex> guard(frames_mutex_);
    if (frames_unused_.empty()) {
      output_frame = new float[x360_frame_samples];
    } else {
      output_frame = frames_unused_.top();
      frames_unused_.pop();
    }
  }

  std::memcpy(output_frame, input_frame, x360_frame_samples * sizeof(float));

  {
    std::unique_lock<std::mutex> guard(frames_mutex_);
    frames_queued_.push(output_frame);
  }
}


void OpenSLESAudioDriver::Shutdown() {
    if (sl_player_play_) {
        (*sl_player_play_)->SetPlayState(sl_player_play_, SL_PLAYSTATE_STOPPED);
    }

    if (sl_player_) {
        (*sl_player_)->Destroy(sl_player_);
        sl_player_ = nullptr;
        sl_player_play_ = nullptr;
        sl_player_buffer_queue_ = nullptr;
        sl_player_effect_send_ = nullptr;
        sl_player_volume_ = nullptr;
    }

    if (sl_output_mix_) {
        (*sl_output_mix_)->Destroy(sl_output_mix_);
        sl_output_mix_ = nullptr;
    }

    if (sl_object_) {
        (*sl_object_)->Destroy(sl_object_);
        sl_object_ = nullptr;
        sl_engine_ = nullptr;
    }

    std::unique_lock<std::mutex> guard(frames_mutex_);
    while (!frames_unused_.empty()) {
        delete[] frames_unused_.top();
        frames_unused_.pop();
    }

    while (!frames_queued_.empty()) {
        delete[] frames_queued_.front();
        frames_queued_.pop();
    }

}

}  // namespace opensles
}  // namespace apu
}  // namespace xe
