// SPDX-License-Identifier: WTFPL

#include "xe_android_disc_swap.h"

#include <algorithm>
#include <condition_variable>
#include <mutex>

#include "xenia/ui/host_disc_swap.h"

namespace xendroid {

namespace {

std::mutex g_mutex;
std::condition_variable g_cv;

uint64_t g_next_id = 1;
bool g_busy = false;
bool g_answered = false;
bool g_accepted = false;
// Counted, not latched: the emulator restarts in-process on title relaunch and
// a latch would leave disc swapping dead for the next title.
uint64_t g_cancel_epoch = 0;

PendingDiscSwap g_request;
std::string g_answer;

std::vector<std::string> g_known_labels;
std::vector<std::string> g_known_paths;

bool Provide(const xe::ui::HostDiscSwapRequest& request,
             xe::ui::HostDiscSwapResult& out_result) {
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
  g_request = PendingDiscSwap{};
  g_request.id = g_next_id++;
  g_request.message = request.message;
  g_request.is_error = request.is_error;
  g_request.disc_number = request.disc_number;
  // The core's own list wins; otherwise use what the host set before boot.
  if (!request.discs.empty()) {
    for (const auto& disc : request.discs) {
      g_request.disc_labels.push_back(disc.label);
      g_request.disc_paths.push_back(disc.path_utf8);
    }
  } else {
    g_request.disc_labels = g_known_labels;
    g_request.disc_paths = g_known_paths;
  }

  g_cv.wait(lock, [&] { return g_answered || g_cancel_epoch != epoch; });

  const bool answered = g_answered;
  out_result.accepted = answered && g_accepted;
  out_result.path_utf8 = answered ? g_answer : std::string();

  g_busy = false;
  g_request = PendingDiscSwap{};
  g_answer.clear();
  g_cv.notify_all();
  return answered;
}

}  // namespace

void InstallDiscSwapProvider() {
  xe::ui::SetHostDiscSwapProvider(&Provide, &CancelAllDiscSwap);
}

void SetKnownDiscs(std::vector<std::string> labels,
                   std::vector<std::string> paths) {
  std::lock_guard<std::mutex> lock(g_mutex);
  // Mismatched lengths would desync label from path; drop the extras.
  const size_t count = std::min(labels.size(), paths.size());
  labels.resize(count);
  paths.resize(count);
  g_known_labels = std::move(labels);
  g_known_paths = std::move(paths);
}

bool PeekDiscSwapRequest(PendingDiscSwap& out_request) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_busy || g_answered) {
    return false;
  }
  out_request = g_request;
  return true;
}

void SubmitDiscSwap(uint64_t id, bool accepted, const std::string& path_utf8) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_busy || g_answered || g_request.id != id) {
    return;
  }
  g_answered = true;
  // An accept with no path is a cancel: there is nothing to mount.
  g_accepted = accepted && !path_utf8.empty();
  g_answer = path_utf8;
  g_cv.notify_all();
}

void CancelAllDiscSwap() {
  std::lock_guard<std::mutex> lock(g_mutex);
  ++g_cancel_epoch;
  g_cv.notify_all();
}

}  // namespace xendroid
