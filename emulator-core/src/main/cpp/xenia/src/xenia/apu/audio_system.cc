/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/apu/audio_system.h"

#include <limits>

#include "xenia/apu/apu_flags.h"
#include "xenia/apu/audio_driver.h"
#include "xenia/apu/xma_decoder.h"
#include "xenia/base/assert.h"
#include "xenia/base/byte_stream.h"
#if XE_PLATFORM_xendroid
#include <dirent.h>
#include <dlfcn.h>
#include <sys/resource.h>
#include <vector>
#endif

#include <atomic>
#include <cerrno>
#include <cstring>

#include "xenia/base/clock.h"
#include "xenia/base/logging.h"
#include "xenia/base/math.h"
#include "xenia/base/profiling.h"
#include "xenia/base/ring_buffer.h"
#include "xenia/base/string_buffer.h"
#include "xenia/base/threading.h"
#include "xenia/cpu/thread_state.h"
#include "xenia/kernel/kernel_state.h"

// As with normal Microsoft, there are like twelve different ways to access
// the audio APIs. Early games use XMA*() methods almost exclusively to touch
// decoders. Later games use XAudio*() and direct memory writes to the XMA
// structures (as opposed to the XMA* calls), meaning that we have to support
// both.
//
// For ease of implementation, most audio related processing is handled in
// AudioSystem, and the functions here call off to it.
// The XMA*() functions just manipulate the audio system in the guest context
// and let the normal AudioSystem handling take it, to prevent duplicate
// implementations. They can be found in xboxkrnl_audio_xma.cc

DECLARE_bool(apu_aaudio_log_stats);

DEFINE_uint32(apu_max_queued_frames, 8,
              "Allows changing max buffered audio frames to reduce audio "
              "delay. Lowering this value might cause performance issues. "
              "Value range: [4-64]",
              "APU");
DEFINE_bool(apu_pump_topup, true,
            "Let the audio buffer refill at up to twice real time while it is "
            "running low, instead of only one frame per 5.33ms interval.",
            "APU");
DEFINE_bool(
    apu_performance_hint, true,
    "Register an ADPF performance hint session over the guest dispatch and "
    "audio threads so the power HAL holds clocks for the audio deadline.",
    "APU");
UPDATE_from_uint32(apu_max_queued_frames, 2024, 8, 31, 20, 64);

namespace xe {
namespace apu {

AudioSystem::AudioSystem(cpu::Processor* processor)
    : memory_(processor->memory()),
      processor_(processor),
      worker_running_(false) {
  queued_frames_ = std::clamp(cvars::apu_max_queued_frames,
                              static_cast<uint32_t>(kMinimumQueuedFrames),
                              static_cast<uint32_t>(kMaximumQueuedFrames));

  for (size_t i = 0; i < kMaximumClientCount; ++i) {
    client_semaphores_[i] = xe::threading::Semaphore::Create(0, queued_frames_);
  }
  pending_work_event_ = xe::threading::Event::CreateAutoResetEvent(false);
  assert_not_null(pending_work_event_);

  xma_decoder_ = std::make_unique<xe::apu::XmaDecoder>(processor_);

  resume_event_ = xe::threading::Event::CreateAutoResetEvent(false);
  assert_not_null(resume_event_);
}

AudioSystem::~AudioSystem() {
  if (xma_decoder_) {
    xma_decoder_->Shutdown();
  }
}

X_STATUS AudioSystem::Setup(kernel::KernelState* kernel_state) {
  X_STATUS result = xma_decoder_->Setup(kernel_state);
  if (result) {
    return result;
  }

  worker_running_ = true;
  worker_thread_ =
      kernel::object_ref<kernel::XHostThread>(new kernel::XHostThread(
          kernel_state, 128 * 1024, 0,
          [this]() {
            WorkerThreadMain();
            return 0;
          },
          kernel_state->GetSystemProcess()));
  // As we run audio callbacks the debugger must be able to suspend us.
  worker_thread_->set_can_debugger_suspend(true);
  worker_thread_->set_name("Audio Worker");
  worker_thread_->Create();
  // Set high priority for this thread for better pacing.
  worker_thread_->SetPriority(24);
  return X_STATUS_SUCCESS;
}

#if XE_PLATFORM_xendroid
// Reporting pump duration against the block deadline is the unprivileged way
// to stop a power profile parking cores below what the mixer needs.
// dlsym'd: ADPF is API 33+, minSdk is 29.
class AudioPerformanceHint {
 public:
  void ReportPump(uint64_t actual_us) {
    if (state_ == State::kDisabled) {
      return;
    }
    if (state_ == State::kUninitialized) {
      Initialize();
      if (state_ == State::kDisabled) {
        return;
      }
    }
    report_(session_, static_cast<int64_t>(actual_us) * 1000);
  }

 private:
  enum class State { kUninitialized, kActive, kDisabled };

  void Initialize() {
    state_ = State::kDisabled;
    if (!cvars::apu_performance_hint) {
      return;
    }
    void* lib = dlopen("libandroid.so", RTLD_NOW | RTLD_NOLOAD);
    if (!lib) {
      return;
    }
    auto get_manager = reinterpret_cast<void* (*)()>(
        dlsym(lib, "APerformanceHint_getManager"));
    auto create_session = reinterpret_cast<void* (*)(
        void*, const int32_t*, size_t, int64_t)>(
        dlsym(lib, "APerformanceHint_createSession"));
    report_ = reinterpret_cast<int (*)(void*, int64_t)>(
        dlsym(lib, "APerformanceHint_reportActualWorkDuration"));
    if (!get_manager || !create_session || !report_) {
      return;
    }
    void* manager = get_manager();
    if (!manager) {
      return;
    }
    std::vector<int32_t> tids;
    if (DIR* dir = opendir("/proc/self/task")) {
      while (dirent* entry = readdir(dir)) {
        int32_t tid = atoi(entry->d_name);
        if (tid <= 0) {
          continue;
        }
        char comm_path[64];
        snprintf(comm_path, sizeof(comm_path), "/proc/self/task/%d/comm", tid);
        char comm[32] = {0};
        if (FILE* f = fopen(comm_path, "r")) {
          fgets(comm, sizeof(comm), f);
          fclose(f);
        }
        if (!strncmp(comm, "Guest CPU", 9) || !strncmp(comm, "Audio Worker", 12) ||
            !strncmp(comm, "XMA Decoder", 11)) {
          tids.push_back(tid);
        }
      }
      closedir(dir);
    }
    if (tids.empty()) {
      return;
    }
    session_ = create_session(manager, tids.data(), tids.size(),
                              AudioSystem::kAudioPumpInterval * int64_t(1000));
    if (!session_) {
      XELOGW("AudioPerformanceHint: createSession failed ({} threads)",
             tids.size());
      return;
    }
    XELOGI("AudioPerformanceHint: session over {} threads, target {}us",
           tids.size(), AudioSystem::kAudioPumpInterval);
    state_ = State::kActive;
  }

  State state_ = State::kUninitialized;
  void* session_ = nullptr;
  int (*report_)(void*, int64_t) = nullptr;
};

// Reports the first failure rather than dropping it: an app that is not
// allowed to renice would otherwise look like it had succeeded.
void ApplyAudioThreadPriority(const char* what) {
  static constexpr int kNice = -12;
  errno = 0;
  if (setpriority(PRIO_PROCESS, 0, kNice) != 0) {
    static std::atomic<bool> reported{false};
    bool expected = false;
    if (reported.compare_exchange_strong(expected, true)) {
      XELOGW("{}: setpriority({}) failed: errno {} ({})", what, kNice, errno,
             std::strerror(errno));
    }
    return;
  }
  const int applied = getpriority(PRIO_PROCESS, 0);
  static std::atomic<bool> logged{false};
  bool expected = false;
  if (logged.compare_exchange_strong(expected, true)) {
    XELOGI("{}: nice set to {}", what, applied);
  }
}
#endif

void AudioSystem::WorkerThreadMain() {
  // Initialize driver and ringbuffer.
  Initialize();

#if XE_PLATFORM_xendroid
  // Set here, not at the creation site: XThread applies its own priority
  // after Create, and its kNormal maps to nice 4, below the Android default.
  // Re-applied in the loop below, because a later set_priority would undo it.
  ApplyAudioThreadPriority("Audio Worker");
  auto priority_last = std::chrono::steady_clock::now();
#endif
#if XE_PLATFORM_xendroid
  AudioPerformanceHint audio_perf_hint;
#endif

  // The host mixer releases a client's semaphore on its own coarse cadence,
  // but Xenos audio subsystem operates at 5.333ms interval (see
  // xaudio2_audio_driver.cc) Interval scales inversely with guest_time_scalar.
  // We therefore pace pumps to each client's next_pump_us deadline and use
  // the semaphore only as back-pressure: a frame is submitted only if
  // a host output slot is free, otherwise it is dropped (the host queue
  // is full, so it is already well buffered).
  while (worker_running_) {
#if XE_PLATFORM_xendroid
    {
      const auto priority_now = std::chrono::steady_clock::now();
      if (priority_now - priority_last >= std::chrono::seconds(1)) {
        priority_last = priority_now;
        ApplyAudioThreadPriority("Audio Worker");
      }
    }
#endif

    const uint64_t now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());

    size_t client_index = kMaximumClientCount;
    uint64_t earliest_pump_us = std::numeric_limits<uint64_t>::max();
    {
      auto global_lock = global_critical_region_.Acquire();

      for (size_t i = 0; i < kMaximumClientCount; ++i) {
        if (!clients_[i].in_use ||
            clients_[i].next_pump_us >= earliest_pump_us) {
          continue;
        }
        earliest_pump_us = clients_[i].next_pump_us;
        client_index = i;
      }

      if (client_index != kMaximumClientCount) {
        const double scalar = xe::Clock::guest_time_scalar();
        const uint64_t min_us =
            scalar > 0.0 ? static_cast<uint64_t>(kAudioPumpInterval / scalar)
                         : kAudioPumpInterval;
        clients_[client_index].next_pump_us =
            (earliest_pump_us > now ? earliest_pump_us : now) + min_us;
      }
    }

    // Test the pause flag on every iteration, not just after a wait on
    // pending_work_event_ returns. When the worker is running behind, the
    // pacing wait below is skipped entirely, so a worker that only noticed a
    // pause through that wait would never acknowledge one and Pause() would
    // block on pause_fence_ forever.
    if (paused_) {
      pause_fence_.Signal();
      xe::threading::Wait(resume_event_.get(), false);
      continue;
    }

    // No clients yet: park until one registers or we're told to stop.
    if (client_index == kMaximumClientCount) {
      xe::threading::Wait(pending_work_event_.get(), true);
      continue;
    }

    // Pace to kAudioIntervalSlack ahead of the deadline.
    const uint64_t wake_target_us = earliest_pump_us > kAudioIntervalSlack
                                        ? earliest_pump_us - kAudioIntervalSlack
                                        : 0;
    if (wake_target_us > now) {
      const std::chrono::milliseconds timeout((wake_target_us - now) / 1000);
      auto result =
          xe::threading::Wait(pending_work_event_.get(), true, timeout);
      if (result == xe::threading::WaitResult::kSuccess) {
        // Woken by a pause or a shutdown; both are handled at the top of the
        // loop, which also re-tests worker_running_ first so a shutdown during
        // a pause does not park on resume_event_.
        continue;
      }

      const uint64_t now_precise = static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now().time_since_epoch())
              .count());
      if (wake_target_us > now_precise) {
        xe::threading::NanoSleepPrecise((wake_target_us - now_precise) * 1000);
      }
    }

    // Submit only if the host has a free output slot; otherwise drop (the host
    // queue is full, so it is already well buffered).
    if (xe::threading::Wait(client_semaphores_[client_index].get(), false,
                            std::chrono::milliseconds(0)) !=
        xe::threading::WaitResult::kSuccess) {
      continue;
    }

    // UnregisterClient waits on this after clearing in_use.
    std::lock_guard<std::mutex> cb_lk(clients_[client_index].callback_mutex);

    uint32_t client_callback = 0;
    uint32_t client_callback_arg = 0;
    {
      auto global_lock = global_critical_region_.Acquire();
      if (clients_[client_index].in_use) {
        client_callback = clients_[client_index].callback;
        client_callback_arg = clients_[client_index].wrapped_callback_arg;
      }
    }

    if (client_callback) {
      SCOPE_profile_cpu_i("apu", "xe::apu::AudioSystem->client_callback");
      uint64_t args[] = {client_callback_arg};
      uint32_t submitted_before =
          clients_[client_index].frames_submitted.load();
#if XE_PLATFORM_xendroid
      auto exec_begin = std::chrono::steady_clock::now();
#endif
      processor_->Execute(worker_thread_->thread_state(), client_callback, args,
                          xe::countof(args));
#if XE_PLATFORM_xendroid
      audio_perf_hint.ReportPump(static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now() - exec_begin)
              .count()));
#endif
      bool submitted =
          clients_[client_index].frames_submitted.load() != submitted_before;

      // Deadline pacing alone submits one block per interval, matching the
      // drain rate, so the sink cannot refill after an underrun. Add at most
      // one extra frame per interval while it is below half full: the guest
      // then runs at most twice real time, briefly. Credits alone are not a
      // usable signal for this - a guest that submits several frames from one
      // callback, or from another thread, breaks the one-credit-one-frame
      // assumption and the buffer gets driven far past real time, which some
      // titles answer with corrupted audio.
      AudioDriver* driver = nullptr;
      {
        auto global_lock = global_critical_region_.Acquire();
        if (clients_[client_index].in_use) {
          driver = clients_[client_index].driver;
        }
      }
      // Refill to above the driver's rate-control depth, or playback slows at
      // a depth this loop is happy to sit at.
      const size_t topup_below = std::max<size_t>(queued_frames_ / 2, 4);
      if (submitted && cvars::apu_pump_topup && driver && !paused_ &&
          worker_running_ && driver->GetQueuedFrameCount() < topup_below &&
          xe::threading::Wait(client_semaphores_[client_index].get(), false,
                              std::chrono::milliseconds(0)) ==
              xe::threading::WaitResult::kSuccess) {
        processor_->Execute(worker_thread_->thread_state(), client_callback,
                            args, xe::countof(args));
      }
    }
  }
  XELOGW("AudioWorker: loop EXITED (worker_running_={})",
         worker_running_.load());
  worker_running_ = false;

  // TODO(benvanik): call module API to kill?
}

int AudioSystem::FindFreeClient() {
  for (int i = 0; i < kMaximumClientCount; i++) {
    auto& client = clients_[i];
    if (!client.in_use) {
      return i;
    }
  }

  return -1;
}

void AudioSystem::Initialize() {}

void AudioSystem::Shutdown() {
  worker_running_ = false;
  pending_work_event_->Set();
  // A worker parked on resume_event_ never sees worker_running_ go false.
  if (paused_) {
    Resume();
  }
  if (worker_thread_) {
    worker_thread_->Wait(0, 0, 0, nullptr);
    worker_thread_.reset();
  }

  // Unregister all active clients to shut down their audio drivers before
  // the semaphores are destroyed with this AudioSystem.
  {
    auto global_lock = global_critical_region_.Acquire();
    for (size_t i = 0; i < kMaximumClientCount; ++i) {
      if (clients_[i].in_use) {
        DestroyDriver(clients_[i].driver);
        if (clients_[i].wrapped_callback_arg) {
          memory()->SystemHeapFree(clients_[i].wrapped_callback_arg);
        }
        clients_[i].driver = nullptr;
        clients_[i].callback = 0;
        clients_[i].callback_arg = 0;
        clients_[i].wrapped_callback_arg = 0;
        clients_[i].in_use = false;
      }
    }
  }
}

X_STATUS AudioSystem::RegisterClient(uint32_t callback, uint32_t callback_arg,
                                     size_t* out_index) {
  auto global_lock = global_critical_region_.Acquire();

  auto index = FindFreeClient();
  assert_true(index >= 0);

  auto client_semaphore = client_semaphores_[index].get();
  auto ret = client_semaphore->Release(queued_frames_, nullptr);
  assert_true(ret);

  AudioDriver* driver;
  auto result = CreateDriver(index, client_semaphore, &driver);
  if (XFAILED(result)) {
    XELOGE("AudioSystem::RegisterClient: CreateDriver failed for index={}",
           index);
    return result;
  }
  assert_not_null(driver);
  XELOGI(
      "AudioSystem::RegisterClient: driver created for index={}, driver={:p}",
      index, (void*)driver);

  uint32_t ptr = memory()->SystemHeapAlloc(0x4);
  xe::store_and_swap<uint32_t>(memory()->TranslateVirtual(ptr), callback_arg);

  clients_[index].driver = driver;
  clients_[index].callback = callback;
  clients_[index].callback_arg = callback_arg;
  clients_[index].wrapped_callback_arg = ptr;
  clients_[index].in_use = true;
  clients_[index].next_pump_us = 0;
  clients_[index].frames_submitted.store(0);
  clients_[index].frames_processed.store(0);
  clients_[index].frames_dropped.store(0);

  // Wake the worker so it re-scans and starts pacing this client immediately.
  pending_work_event_->Set();

  XELOGI("AudioSystem::RegisterClient: client {} registered successfully",
         index);

  if (out_index) {
    *out_index = index;
  }

  return X_STATUS_SUCCESS;
}

void AudioSystem::SubmitFrame(size_t index, float* samples) {
  SCOPE_profile_cpu_f("apu");

  auto global_lock = global_critical_region_.Acquire();
  assert_true(index < kMaximumClientCount);
  if (index >= kMaximumClientCount || !clients_[index].in_use ||
      !clients_[index].driver) {
    XELOGW(
        "SubmitFrame called for invalid/unregistered client index {} "
        "(in_use={}, driver={:p})",
        index, index < kMaximumClientCount ? clients_[index].in_use : false,
        index < kMaximumClientCount ? (void*)clients_[index].driver : nullptr);

    // Submit silence instead of dropping the frame to maintain the callback
    // chain.  If we don't submit anything, the audio driver's OnBufferEnd
    // callback will never fire, causing the semaphore to leak.
    if (index < kMaximumClientCount && clients_[index].driver) {
      static float silence[apu::AudioDriver::kFrameSamplesMax] = {0};
      clients_[index].frames_dropped++;
      (clients_[index].driver)->SubmitFrame(silence);
    } else if (index < kMaximumClientCount) {
      // Tick the semaphore so the worker doesn't stall on a dead client.
      client_semaphores_[index]->Release(1, nullptr);
    }
    return;
  }
  clients_[index].frames_submitted++;
  clients_[index].frames_processed++;
  (clients_[index].driver)->SubmitFrame(samples);
}

bool AudioSystem::GetClientPerformance(size_t index,
                                       ClientPerformance* out_perf) {
  if (index >= kMaximumClientCount || !out_perf) {
    return false;
  }

  if (!clients_[index].in_use) {
    return false;
  }

  out_perf->frames_submitted = clients_[index].frames_submitted.load();
  out_perf->frames_processed = clients_[index].frames_processed.load();
  out_perf->frames_dropped = clients_[index].frames_dropped.load();
  return true;
}

void AudioSystem::UnregisterClient(size_t index) {
  SCOPE_profile_cpu_f("apu");

  assert_true(index < kMaximumClientCount);
  AudioDriver* driver_to_destroy;
  {
    auto global_lock = global_critical_region_.Acquire();
    XELOGI(
        "AudioSystem::UnregisterClient: index={}, driver={:p}", index,
        index < kMaximumClientCount ? (void*)clients_[index].driver : nullptr);
    driver_to_destroy = clients_[index].driver;
    // Leak wrapped_callback_arg: in-flight callback may hold this pointer.
    clients_[index].driver = nullptr;
    clients_[index].callback = 0;
    clients_[index].callback_arg = 0;
    clients_[index].wrapped_callback_arg = 0;
    clients_[index].in_use = false;
    clients_[index].next_pump_us = 0;
    clients_[index].frames_submitted.store(0);
    clients_[index].frames_processed.store(0);
    clients_[index].frames_dropped.store(0);
  }

  // Wait for any in-flight callback; can't hold global lock (callback
  // re-enters).
  {
    std::lock_guard<std::mutex> lk(clients_[index].callback_mutex);
  }

  DestroyDriver(driver_to_destroy);

  // Drain the semaphore of its count.
  auto client_semaphore = client_semaphores_[index].get();
  xe::threading::WaitResult wait_result;
  do {
    wait_result = xe::threading::Wait(client_semaphore, false,
                                      std::chrono::milliseconds(0));
  } while (wait_result == xe::threading::WaitResult::kSuccess);
  assert_true(wait_result == xe::threading::WaitResult::kTimeout);
}

bool AudioSystem::Save(ByteStream* stream) {
  stream->Write(kAudioSaveSignature);

  // Count the number of used clients first.
  // Any gaps should be handled gracefully.
  uint32_t used_clients = 0;
  for (int i = 0; i < kMaximumClientCount; i++) {
    if (clients_[i].in_use) {
      used_clients++;
    }
  }

  stream->Write(used_clients);
  for (uint32_t i = 0; i < kMaximumClientCount; i++) {
    auto& client = clients_[i];
    if (!client.in_use) {
      continue;
    }

    stream->Write(i);
    stream->Write(client.callback);
    stream->Write(client.callback_arg);
    stream->Write(client.wrapped_callback_arg);
  }

  return true;
}

bool AudioSystem::Restore(ByteStream* stream) {
  if (stream->Read<uint32_t>() != kAudioSaveSignature) {
    XELOGE("AudioSystem::Restore - Invalid magic value!");
    return false;
  }

  uint32_t num_clients = stream->Read<uint32_t>();
  for (uint32_t i = 0; i < num_clients; i++) {
    auto id = stream->Read<uint32_t>();
    assert_true(id < kMaximumClientCount);

    auto& client = clients_[id];

    // Reset the semaphore and recreate the driver ourselves.
    if (client.driver) {
      UnregisterClient(id);
    }

    client.callback = stream->Read<uint32_t>();
    client.callback_arg = stream->Read<uint32_t>();
    client.wrapped_callback_arg = stream->Read<uint32_t>();

    client.next_pump_us = 0;
    client.in_use = true;

    auto client_semaphore = client_semaphores_[id].get();
    auto ret = client_semaphore->Release(queued_frames_, nullptr);
    assert_true(ret);

    AudioDriver* driver = nullptr;
    auto status = CreateDriver(id, client_semaphore, &driver);
    if (XFAILED(status)) {
      XELOGE(
          "AudioSystem::Restore - Call to CreateDriver failed with status "
          "{:08X}",
          status);
      return false;
    }

    assert_not_null(driver);
    client.driver = driver;
  }

  return true;
}

void AudioSystem::Pause() {
  if (paused_) {
    return;
  }
  paused_ = true;

  // Kind of a hack, but it works.
  pending_work_event_->Set();
  pause_fence_.Wait();

  // Only once the worker is parked: a running sink drains a queue nothing is
  // refilling, which reads as a starving producer.
  {
    auto global_lock = global_critical_region_.Acquire();
    for (size_t i = 0; i < kMaximumClientCount; ++i) {
      if (clients_[i].in_use && clients_[i].driver) {
        clients_[i].driver->Pause();
      }
    }
  }

  xma_decoder_->Pause();
}

void AudioSystem::Resume() {
  if (!paused_) {
    return;
  }
  paused_ = false;

  {
    auto global_lock = global_critical_region_.Acquire();
    for (size_t i = 0; i < kMaximumClientCount; ++i) {
      if (clients_[i].in_use && clients_[i].driver) {
        clients_[i].driver->Resume();
      }
    }
  }

  resume_event_->Set();

  xma_decoder_->Resume();
}

}  // namespace apu
}  // namespace xe
