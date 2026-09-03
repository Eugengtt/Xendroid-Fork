// SPDX-License-Identifier: WTFPL

#include "xe_android_message_box.h"

#include <condition_variable>
#include <mutex>

#include "xenia/ui/host_message_box.h"

namespace xendroid {

namespace {

std::mutex g_mutex;
std::condition_variable g_cv;

uint64_t g_next_id = 1;
bool g_busy = false;
bool g_answered = false;
uint32_t g_answer = 0;
// Counted, not latched: the emulator restarts in-process on relaunch, and a
// latch would leave message boxes dead for the next title.
uint64_t g_cancel_epoch = 0;

PendingMessageBox g_request;

bool Provide(const xe::ui::HostMessageBoxRequest& request,
             xe::ui::HostMessageBoxResult& out_result) {
  std::unique_lock<std::mutex> lock(g_mutex);
  const uint64_t epoch = g_cancel_epoch;

  // One panel at a time.
  g_cv.wait(lock, [&] { return !g_busy || g_cancel_epoch != epoch; });
  if (g_cancel_epoch != epoch) {
    return false;
  }

  g_busy = true;
  g_answered = false;
  g_answer = 0;
  g_request = PendingMessageBox{};
  g_request.id = g_next_id++;
  g_request.title = request.title;
  g_request.text = request.text;
  g_request.buttons = request.buttons;
  g_request.active_button = request.active_button;
  g_request.flags = request.flags;

  g_cv.wait(lock, [&] { return g_answered || g_cancel_epoch != epoch; });

  const bool answered = g_answered;
  if (answered) {
    out_result.chosen_button = g_answer;
  }

  g_busy = false;
  g_request = PendingMessageBox{};
  g_answer = 0;
  g_cv.notify_all();
  return answered;
}

}  // namespace

void InstallMessageBoxProvider() {
  xe::ui::SetHostMessageBoxProvider(&Provide, &CancelAllMessageBox);
}

bool PeekMessageBoxRequest(PendingMessageBox& out_request) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_busy || g_answered) {
    return false;
  }
  out_request = g_request;
  return true;
}

void SubmitMessageBox(uint64_t id, uint32_t chosen_button) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_busy || g_answered || g_request.id != id) {
    return;
  }
  g_answered = true;
  // Out of range would index past the guest's own button array.
  g_answer = chosen_button < g_request.buttons.size() ? chosen_button
                                                      : g_request.active_button;
  g_cv.notify_all();
}

void CancelAllMessageBox() {
  std::lock_guard<std::mutex> lock(g_mutex);
  ++g_cancel_epoch;
  g_cv.notify_all();
}

}  // namespace xendroid
