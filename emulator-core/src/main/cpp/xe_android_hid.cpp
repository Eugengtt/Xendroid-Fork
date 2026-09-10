/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xe_android_hid.h"
#include "xe_android_input_driver.h"
namespace xe {
    namespace hid {
        namespace android {
            std::unique_ptr<InputDriver> Create(xe::ui::Window* window,size_t window_z_order) {
                return std::make_unique<AndroidInputDriver>(window, window_z_order);
            }
        }
    }
}