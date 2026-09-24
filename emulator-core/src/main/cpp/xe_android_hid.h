/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef xendroid_XE_ANDROID_HID_H
#define xendroid_XE_ANDROID_HID_H


#include <memory>

#include "xenia/hid/input_system.h"

namespace xe {
    namespace hid {
        namespace android {
            std::unique_ptr<InputDriver> Create(xe::ui::Window* window,
                                                size_t window_z_order);
        }
    }
}

#endif //xendroid_XE_ANDROID_HID_H
