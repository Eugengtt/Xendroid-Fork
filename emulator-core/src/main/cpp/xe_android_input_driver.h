/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef xendroid_XE_ANDROID_INPUT_DRIVER_H
#define xendroid_XE_ANDROID_INPUT_DRIVER_H

#include <mutex>
#include <queue>
#include <vector>

#include "xenia/base/mutex.h"
#include "xenia/hid/input_driver.h"
#include "xenia/ui/virtual_key.h"

namespace xe {
    namespace hid {
        namespace android {

            class AndroidInputDriver final : public InputDriver {
            public:
                struct KeyStatus{
                    ui::VirtualKey id;
                    bool pressed;
                    int value;
                };

                // Deliberately not the global critical region: analog axes push a
                // sample per motion event and nothing here needs its semantics.
                std::mutex key_status_mutex_;

                std::array<KeyStatus, 24> key_status_;
                std::array<KeyStatus, 24> prev_key_status_;

                uint32_t key_status_mask_ = 0;

                uint32_t packet_number_ = 1;

                explicit AndroidInputDriver(xe::ui::Window* window, size_t window_z_order);
                ~AndroidInputDriver() override;

                X_STATUS Setup() override;

                X_RESULT GetCapabilities(uint32_t user_index, uint32_t flags,X_INPUT_CAPABILITIES* out_caps) override;
                X_RESULT GetState(uint32_t user_index, X_INPUT_STATE* out_state) override;
                X_RESULT SetState(uint32_t user_index, X_INPUT_VIBRATION* vibration) override;
                X_RESULT GetKeystroke(uint32_t user_index, uint32_t flags,X_INPUT_KEYSTROKE* out_keystroke) override;

                InputType GetInputType() const override;
                // XenDroid: edge's InputSystem builds guest controller-slot
                // bindings from each driver's EnumerateDevices(). Advertise the
                // always-present Android controller so it auto-binds to a slot.
                std::vector<InputDeviceInfo> EnumerateDevices() override;
                void OnKey(int key_index, bool pressed, short value);

            };
        }
    }
}

#endif //xendroid_XE_ANDROID_INPUT_DRIVER_H
