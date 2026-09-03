// SPDX-License-Identifier: WTFPL

#include "xe_android_text_input.h"

#include <condition_variable>
#include <mutex>

#include "xenia/ui/host_text_input.h"

namespace xendroid {

namespace {

std::mutex g_mutex;
std::condition_variable g_cv;

uint64_t g_next_id = 1;
bool g_busy = false;
bool g_answered = false;
bool g_accepted = false;
// Counted, not latched: the emulator restarts in-process on title relaunch and
// a latch would leave text entry dead for the next title.
uint64_t g_cancel_epoch = 0;

PendingTextInput g_request;
std::string g_answer;

bool Provide(const xe::ui::HostTextInputRequest& request,
             xe::ui::HostTextInputResult& out_result) {
  std::unique_lock<std::mutex> lock(g_mutex);
  const uint64_t epoch = g_cancel_epoch;

  // One panel at a time.
  g_cv.wait(lock, [&] { return !g_busy || g_cancel_epoch != epoch; });
  if (g_cancel_epoch != epoch) {
    return false;
  }

  g_busy = true;
  g_answered = false;
  g_accepted = false;
  g_answer.clear();
  g_request = PendingTextInput{g_next_id++,          request.title,
                               request.description,  request.default_text,
                               request.max_length,   request.flags};

  g_cv.wait(lock, [&] { return g_answered || g_cancel_epoch != epoch; });

  const bool answered = g_answered;
  out_result.accepted = answered && g_accepted;
  out_result.text = answered ? g_answer : std::string();

  g_busy = false;
  g_request = PendingTextInput{};
  g_answer.clear();
  g_cv.notify_all();
  return answered;
}

}  // namespace

void InstallTextInputProvider() {
  xe::ui::SetHostTextInputProvider(&Provide, &CancelAllTextInput);
}

bool PeekTextInputRequest(PendingTextInput& out_request) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_busy || g_answered) {
    return false;
  }
  out_request = g_request;
  return true;
}

void SubmitTextInput(uint64_t id, bool accepted, const std::string& text_utf8) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_busy || g_answered || g_request.id != id) {
    return;
  }
  g_answered = true;
  g_accepted = accepted;
  g_answer = text_utf8;
  g_cv.notify_all();
}

void CancelAllTextInput() {
  std::lock_guard<std::mutex> lock(g_mutex);
  ++g_cancel_epoch;
  g_cv.notify_all();
}

}  // namespace xendroid
