/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_INPUT_DRIVER_H_
#define XENIA_HID_INPUT_DRIVER_H_

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "xenia/hid/input.h"
#include "xenia/ui/window.h"
#include "xenia/xbox.h"

namespace xe {
namespace ui {
class Window;
}  // namespace ui
}  // namespace xe

namespace xe {
namespace hid {

class InputSystem;

enum InputType { None, Controller = 1, Keyboard = 2, Other = 4 };

// Identifies one physical input device the driver currently knows about.
struct InputDeviceInfo {
  uint8_t driver_slot;       // pass back into GetState/SetState/etc.
  std::string stable_id;     // for cross-disconnect identification; "" if none
  std::string display_name;  // for UI
  // XINPUT_DEVSUBTYPE_* value; lets UI pick an icon without re-querying SDL.
  uint8_t subtype = 0x01;      // XINPUT_DEVSUBTYPE_GAMEPAD
  int8_t preferred_slot = -1;  // initial guest slot hint; -1 = no preference
  // True if InputSystem may auto-bind this device to a free slot on first
  // encounter. Devices that should only be bound by explicit user action set
  // this to false.
  bool auto_bind = true;
};

class InputDriver {
 public:
  virtual ~InputDriver() = default;

  virtual X_STATUS Setup() = 0;

  virtual X_RESULT GetCapabilities(uint32_t user_index, uint32_t flags,
                                   X_INPUT_CAPABILITIES* out_caps) = 0;
  virtual X_RESULT GetState(uint32_t user_index, X_INPUT_STATE* out_state) = 0;
  virtual X_RESULT SetState(uint32_t user_index,
                            X_INPUT_VIBRATION* vibration) = 0;
  virtual X_RESULT GetKeystroke(uint32_t user_index, uint32_t flags,
                                X_INPUT_KEYSTROKE* out_keystroke) = 0;

  virtual InputType GetInputType() const = 0;

  // Default: no enumerable devices. Drivers that surface devices override.
  virtual std::vector<InputDeviceInfo> EnumerateDevices() { return {}; }

  // Hooks called by InputSystem when slot bindings change. Drivers can
  // override to persist their own state (e.g. cvars) in response.
  virtual void OnBoundToSlot(uint8_t /*driver_slot*/, uint32_t /*guest_slot*/) {
  }
  virtual void OnUnboundFromSlot(uint8_t /*driver_slot*/) {}

  // Drivers invoke NotifyDevicesChanged() after their EnumerateDevices result
  // changes (hotplug add/remove). InputSystem registers this callback during
  // AddDriver to reconcile the binding table in response.
  using DevicesChangedCallback = std::function<void()>;
  void set_devices_changed_callback(DevicesChangedCallback cb) {
    devices_changed_callback_ = std::move(cb);
  }

 protected:
  explicit InputDriver(xe::ui::Window* window, size_t window_z_order)
      : window_(window), window_z_order_(window_z_order) {}

  xe::ui::Window* window() const { return window_; }
  size_t window_z_order() const { return window_z_order_; }

  void NotifyDevicesChanged() {
    if (devices_changed_callback_) {
      devices_changed_callback_();
    }
  }

 private:
  xe::ui::Window* window_;
  size_t window_z_order_;
  DevicesChangedCallback devices_changed_callback_;
};

}  // namespace hid
}  // namespace xe

#endif  // XENIA_HID_INPUT_DRIVER_H_
