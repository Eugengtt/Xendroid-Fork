/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#ifndef xendroid_XE_OPENSLES_AUDIO_DRIVER_H
#define xendroid_XE_OPENSLES_AUDIO_DRIVER_H

#include <mutex>
#include <queue>
#include <stack>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "xenia/apu/audio_driver.h"
#include "xenia/base/threading.h"

namespace xe {
    namespace apu {
        namespace opensles {

            class OpenSLESAudioDriver : public AudioDriver {
            public:
                OpenSLESAudioDriver(Memory* memory, xe::threading::Semaphore* semaphore);
                ~OpenSLESAudioDriver() override;

                bool Initialize();
                void Pause() override;
                void Resume() override;
                void SetVolume(float volume) override;
                void SubmitFrame(float* frame) override;
                void Shutdown();


            protected:

                static void PlayerCallback(SLAndroidSimpleBufferQueueItf buffer_queue, void* context);

                xe::threading::Semaphore* semaphore_ = nullptr;

                SLObjectItf sl_object_= nullptr;
                SLEngineItf sl_engine_= nullptr;
                SLObjectItf sl_output_mix_= nullptr;
                SLObjectItf sl_player_= nullptr;
                SLPlayItf sl_player_play_= nullptr;
                SLAndroidSimpleBufferQueueItf sl_player_buffer_queue_= nullptr;
                SLEffectSendItf sl_player_effect_send_= nullptr;
                SLVolumeItf sl_player_volume_= nullptr;

                static constexpr uint32_t frame_frequency_ = 48000;
                static constexpr uint32_t x360_frame_channels_ = 6;
                static constexpr uint32_t channel_samples_ = 256;
                static constexpr uint32_t host_frame_channels_ = 2;
                std::mutex frames_mutex_ = {};

                std::queue<float*> frames_queued_ = {};
                std::stack<float*> frames_unused_ = {};

            };

        }
    }
}
#endif //xendroid_XE_OPENSLES_AUDIO_DRIVER_H
