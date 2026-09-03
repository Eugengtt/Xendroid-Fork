/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <jni.h>

#include <android/asset_manager.h>
#include <android/configuration.h>
#include <android/looper.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <jni.h>
#include <array>
#include <filesystem>
#include <memory>
#include <sys/prctl.h>

#include "xenia/apu/nop/nop_audio_system.h"
#include "xenia/base/cvar.h"
#include "xenia/base/logging.h"
#include "xenia/base/profiling.h"
#include "xenia/config.h"
#include "xenia/emulator.h"
#include "xenia/gpu/graphics_system.h"
#include "xenia/gpu/null/null_graphics_system.h"
#include "xenia/gpu/vulkan/vulkan_graphics_system.h"
#include "xenia/hid/nop/nop_hid.h"
#include "xenia/kernel/xam/xam_module.h"
#include "xenia/vfs/devices/host_path_device.h"

#include "emulator.h"
#include "emulator_xendroid.h"

#include "xe_android_hid.h"
#include "xe_android_input_driver.h"
#include "xe_android_disc_swap.h"
#include "xe_android_message_box.h"
#include "xe_android_text_input.h"
#include "xe_opensles_audio_system.h"
#include "xe_aaudio_audio_system.h"

#include "xendroid_emu.h"
//#include "nlohmann/json.hpp"

#define LOG_TAG "xendroid_native"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG,__VA_ARGS__);

DEFINE_string(apu, "aaudio", "Audio system. Use: [any, nop, opensles, aaudio]", "APU");
DEFINE_string(gpu, "vulkan", "Graphics system. Use: [vulkan, null]",
              "GPU");
DEFINE_bool(android_soft_keyboard, true,
            "Show the Android keyboard when a game asks for text input, "
            "instead of answering the prompt with its default text.",
            "UI");
DEFINE_bool(android_disc_swap, true,
            "Ask which disc to insert when a multi-disc game requests one, "
            "instead of leaving the drive empty.",
            "UI");
DEFINE_bool(android_message_box, true,
            "Show a game's message boxes as an Android dialog, instead of "
            "answering them with the button the game pre-selected.",
            "UI");
DEFINE_string(hid, "android", "Input system. Use: [android, nop]",
              "HID");
DEFINE_bool(show_touch_overlay, true,
            "Draw the on-screen controller. Seeded on first launch from whether "
            "a physical controller was attached, and honoured as-is after that.",
            "HID");

DEFINE_path(
        storage_root, "",
        "Root path for persistent internal data storage (config, etc.), or empty "
        "to use the path preferred for the OS, such as the documents folder, or "
        "the emulator executable directory if portable.txt is present in it.",
        "Storage");
DEFINE_path(
        content_root, "",
        "Root path for guest content storage (saves, etc.), or empty to use the "
        "content folder under the storage root.",
        "Storage");
DEFINE_path(
        cache_root, "",
        "Root path for files used to speed up certain parts of the emulator or the "
        "game. These files may be persistent, but they can be deleted without "
        "major side effects such as progress loss. If empty, the cache folder "
        "under the storage root, or, if available, the cache directory preferred "
        "for the OS, will be used.",
        "Storage");

DEFINE_bool(mount_scratch, false, "Enable scratch mount", "Storage");
DEFINE_bool(mount_cache, false, "Enable cache mount", "Storage");
DEFINE_bool(mount_memory_unit, false, "Enable memory unit (MU) mount",
            "Storage");

DECLARE_bool(force_mount_devkit);
DEFINE_transient_path(target, "",
                      "Specifies the target .xex or .iso to execute.",
                      "General");
DEFINE_transient_bool(portable, false,
                      "Specifies if Xenia should run in portable mode.",
                      "General");

DECLARE_bool(debug);
// Defined in xenia/ui/presenter.cc. On Android this MUST be true: when false the
// Presenter routes to kUIThreadOnRequest (presenter.cc), whose fire-and-forget
// request_paint() never wakes the UI loop here, so guest frames never present
// (black screen with working CPU/audio). We force it true after config load so a
// stale/edited global config can't black-screen the app.
DECLARE_bool(host_present_from_non_ui_thread);

DEFINE_bool(discord, false, "Enable Discord rich presence", "General");

extern JavaVM* g_jvm;
namespace ae{
    extern std::unique_ptr<xe::ui::WindowedApp> g_windowed_app;
}
void AndroidWindowedAppContext::NotifyUILoopOfPendingFunctions() {
    // Only ever called from non-UI threads (emu_thr at boot, binder thread for
    // surface ops) -- NEVER the GPU present thread -- so blocking here cannot
    // recreate the request_paint() AB-BA deadlock. Preserves the fork's
    // synchronous CallInUIThread contract that boot relies on.
    assert(WindowedAppContext::ui_thread_id_!=std::this_thread::get_id());
    pthread_mutex_lock(&mutex);
    uint64_t ticket = ++exec_requested;
    pending_events |= EVENT_EXECUTE_PENDING_FUNCTIONS;
    while(exec_completed < ticket && !ui_loop_exited){
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);
}

void AndroidWindowedAppContext::PlatformQuitFromUIThread() {
    if(WindowedAppContext::ui_thread_id_==std::this_thread::get_id()){
        pthread_mutex_lock(&mutex);
        pending_events |= EVENT_QUIT;
        pthread_mutex_unlock(&mutex);
        return;
    }
    pthread_mutex_lock(&mutex);
    pending_events |= EVENT_QUIT;
    while((pending_events & EVENT_QUIT) && !ui_loop_exited){
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);
}

void AndroidWindowedAppContext::request_paint() {
    // FIRE-AND-FORGET. Must NEVER block: the GPU present thread calls this from
    // Presenter::PaintAndPresent while holding paint_mode_mutex_. Blocking here
    // (the old behavior) is the AB-BA deadlock -- main_thr would wait on the
    // pump while the GPU thread waits on main_thr holding paint_mode_mutex_.
    pthread_mutex_lock(&mutex);
    pending_events |= EVENT_PAINT;
    pthread_mutex_unlock(&mutex);
}

void AndroidWindowedAppContext::setup_ui_thr_id(std::thread::id id){
    WindowedAppContext::ui_thread_id_=id;
}

void AndroidWindowedAppContext::main_loop(){
    assert(WindowedAppContext::ui_thread_id_==std::this_thread::get_id());
    while(!WindowedAppContext::HasQuitFromUIThread()){
        // Snapshot the pending bits, then run all work UNLOCKED. Holding `mutex`
        // across ExecutePendingFunctions / Paint would self-deadlock: those can
        // call request_paint() -> pthread_mutex_lock(&mutex) on this same thread.
        pthread_mutex_lock(&mutex);
        uint32_t ev = pending_events;
        // PAINT is fire-and-forget; consume it now. EXECUTE is consumed here too
        // but its waiters block on exec_completed, signalled AFTER the run below.
        pending_events &= ~(EVENT_PAINT | EVENT_EXECUTE_PENDING_FUNCTIONS);
        uint64_t exec_to = exec_requested;
        pthread_mutex_unlock(&mutex);

        if(ev == 0){
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        if(ev & EVENT_EXECUTE_PENDING_FUNCTIONS){
            WindowedAppContext::ExecutePendingFunctionsFromUIThread();
            // Release every producer whose ticket was taken before this drain.
            pthread_mutex_lock(&mutex);
            exec_completed = exec_to;
            pthread_cond_broadcast(&cond);
            pthread_mutex_unlock(&mutex);
        }

        if(ev & EVENT_PAINT){
            EmulatorApp* app=reinterpret_cast<EmulatorApp*>(ae::g_windowed_app.get());
            // emu_window is an EmulatorWindow, NOT an AndroidWindow -- the real
            // ui::Window (the AndroidWindow that SetPresenter() was called on) is
            // emu_window->window(). A reinterpret_cast here would read presenter_ at
            // the wrong offset (latent UB), so go through emu_window->window().
            AndroidWindow* win = app->emu_window
                ? static_cast<AndroidWindow*>(app->emu_window->window()) : nullptr;
            if(win) win->Paint();
        }

        if(ev & EVENT_QUIT){
            // Drain remaining queued functions, then fall through to the
            // shutdown epilogue (which releases any stranded synchronous waiter).
            WindowedAppContext::QuitFromUIThread();
            break;
        }
    }

    // Loop is exiting (quit). Release every producer still blocked in
    // NotifyUILoopOfPendingFunctions / PlatformQuitFromUIThread so a synchronous
    // surface_detach (or any CallInUIThread) can't hang forever during teardown --
    // their closures were already drained by QuitFromUIThread or a prior iteration.
    // (Guards against a lost wakeup when quit races a synchronous detach.)
    pthread_mutex_lock(&mutex);
    ui_loop_exited = true;
    exec_completed = exec_requested;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
}

AndroidWindowedAppContext::AndroidWindowedAppContext() {
    pthread_mutex_init(&mutex, nullptr);
    pthread_cond_init(&cond, nullptr);
}

AndroidWindowedAppContext::~AndroidWindowedAppContext(){
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);
}

AndroidWindow::AndroidWindow(xe::ui::WindowedAppContext& app_context, const std::string_view title,
                             uint32_t desired_logical_width, uint32_t desired_logical_height)
                             : Window(app_context, title, desired_logical_width, desired_logical_height) {}

bool AndroidWindow::OpenImpl() {
    XELOGI("Opening Android window...");
    return true;
}

void AndroidWindow::RequestCloseImpl() {
    XELOGI("Requesting Android window close...");
}

std::unique_ptr<xe::ui::Surface> AndroidWindow::CreateSurfaceImpl(xe::ui::Surface::TypeFlags allowed_types) {
    XELOGI("Creating Android surface...");
    if(allowed_types&xe::ui::Surface::kTypeFlag_AndroidNativeWindow) {
        std::lock_guard<std::mutex> lk(ae::window_mutex);
        ANativeWindow *window = ae::window;
        return std::make_unique<xe::ui::AndroidNativeWindowSurface>(window);
    }
    return nullptr;
}

void AndroidWindow::RequestPaintImpl() {
    XELOGI("Requesting Android window paint...");
    AndroidWindowedAppContext* context=static_cast<AndroidWindowedAppContext*>(&app_context());
    context->request_paint();
}

void AndroidWindow::UpdateSurface(){
    // Detach the old presenter surface (if any) and create+attach a new one
    // from ae::window. All GPU drain + swapchain rebuild happens inside
    // Presenter::SetWindowSurfaceFromUIThread. UI-thread only.
    OnSurfaceChanged(true);
}

void AndroidWindow::DetachSurface(){
    // Detach the presenter from the current surface WITHOUT creating a new one
    // (drains the GPU + destroys swapchain/VkSurfaceKHR). UI-thread only.
    OnSurfaceChanged(false);
}

void AndroidWindow::Paint(){
    OnPaint(false);
}

std::unique_ptr<xe::ui::Window> xe::ui::Window::Create(WindowedAppContext& app_context,
                                                       const std::string_view title,
                                                       uint32_t desired_logical_width,
                                                       uint32_t desired_logical_height) {
    return std::make_unique<AndroidWindow>(
            app_context, title, desired_logical_width, desired_logical_height);
}

android_menu_item::android_menu_item(Type type, const std::string& text, const std::string& hotkey,
                                     std::function<void()> callback)
        : MenuItem(type, text, hotkey, callback) {
    LOGW("android_menu_item: %d %s %s",static_cast<int>(type),text.c_str(),hotkey.c_str());
}

std::unique_ptr<xe::ui::MenuItem> xe::ui::MenuItem::Create(Type type,
                                                   const std::string& text,
                                                   const std::string& hotkey,
                                                   std::function<void()> callback) {
    return std::make_unique<android_menu_item>(type, text, hotkey, callback);
}


std::unique_ptr<xe::ui::WindowedApp> EmulatorApp::create(xe::ui::WindowedAppContext& app_context) {
    return std::unique_ptr<xe::ui::WindowedApp>(new EmulatorApp(app_context));
}

EmulatorApp::EmulatorApp(xe::ui::WindowedAppContext& app_context)
        : WindowedApp(app_context,"ax36e") {
}

bool EmulatorApp::OnInitialize() {

    xe::Profiler::Initialize();
    xe::Profiler::ThreadEnter("Main");

    std::filesystem::path storage_root=cvars::storage_root;

    storage_root = std::filesystem::absolute(storage_root);
    XELOGI("Storage root: {}", storage_root.c_str());

    config::SetupConfig(storage_root);

    // Apply the per-game config overlay (config/<TITLEID>.config.toml) onto the live cvars, like
    // desktop's launch path (xenia_main.cc). Must run after the global load and before any cvar is
    // read; the present-mode force below runs after, so it still wins.
    if (!cvars::target.empty()) {
        std::string game_path = cvars::target;
        config::LoadGameConfigForFile(
            std::filesystem::absolute(std::filesystem::u8path(game_path)));
    }

    // Android has no UI-thread paint pump that the kUIThreadOnRequest present mode
    // needs, so this must be true regardless of what the (possibly stale/edited)
    // global config says -- otherwise nothing presents (black screen, audio/input OK).
    cvars::host_present_from_non_ui_thread = true;

    // Must precede any title boot: without a provider XamShowKeyboardUI answers
    // itself with the title's own default text.
    if (cvars::android_soft_keyboard) {
        xendroid::InstallTextInputProvider();
    }
    // Likewise for XamSwapDisc: with no provider the ImGui dialog is drawn but
    // can never be dismissed, because Android never dispatches input into
    // ui::Window.
    if (cvars::android_disc_swap) {
        xendroid::InstallDiscSwapProvider();
    }
    // Otherwise headless silently answers with the game's own default button.
    if (cvars::android_message_box) {
        xendroid::InstallMessageBoxProvider();
    }

#if XE_ARCH_AMD64 == 1
    xe::amd64::InitFeatureFlags();
#elif XE_ARCH_ARM64 == 1
    xe::arm64::InitFeatureFlags();
#endif

    std::filesystem::path content_root = cvars::content_root;
    if (content_root.empty()) {
        content_root = storage_root / "content";
    } else {
        // If content root isn't an absolute path, then it should be relative to the
        // storage root.
        if (!content_root.is_absolute()) {
            content_root = storage_root / content_root;
        }
    }
    content_root = std::filesystem::absolute(content_root);
    XELOGI("Content root: {}", content_root.c_str());

    std::filesystem::path cache_root = cvars::cache_root;
    if (cache_root.empty()) {
        cache_root = storage_root / "cache";
        // TODO(Triang3l): Point to the app's external storage "cache" directory on
        // Android.
    } else {
        // If content root isn't an absolute path, then it should be relative to the
        // storage root.
        if (!cache_root.is_absolute()) {
            cache_root = storage_root / cache_root;
        }
    }
    cache_root = std::filesystem::absolute(cache_root);
    XELOGI("Host cache root: {}", cache_root);

    // Create the emulator but don't initialize so we can setup the window.
    emu =
            std::make_unique<xe::Emulator>("", storage_root, content_root, cache_root);

    // Window size comes from the actual Android surface (set via
    // ANativeWindow_getWidth/Height in emulator.cpp); the fork-side
    // GraphicsSystem::GetInternalDisplayResolution() was dropped in favor of
    // upstream's XConfig-based GetResolution().

    // Main emulator display window. Snapshot the surface dimensions under the
    // window guard -- the binder thread can write them via surface_attach/resize.
    int boot_w, boot_h;
    {
        std::lock_guard<std::mutex> lk(ae::window_mutex);
        boot_w = ae::window_width;
        boot_h = ae::window_height;
    }
    emu_window = xe::app::EmulatorWindow::Create(emu.get(), app_context(),
                                                 boot_w, boot_h);

    if (!emu_window) {
        XELOGE("Failed to create the main emulator window");
        return false;
    }

    emu_thr_quit_requested.store(false, std::memory_order_relaxed);
    emu_thr_event = xe::threading::Event::CreateAutoResetEvent(false);
    assert_not_null(emu_thr_event);
    emu_thr = std::thread(&EmulatorApp::emu_thr_main, this);

    return true;
}

std::unique_ptr<xe::apu::AudioSystem> EmulatorApp::create_audio_system(xe::cpu::Processor* processor) {
    Factory<xe::apu::AudioSystem, xe::cpu::Processor*> factory;
    factory.Add<xe::apu::nop::NopAudioSystem>("nop");
#if XE_PLATFORM_xendroid
    factory.Add<xe::apu::opensles::OpenSLESAudioSystem>("opensles");
    factory.Add<xe::apu::aaudio::AAudioAudioSystem>("aaudio");
#endif
    return factory.Create(cvars::apu, processor);
}

std::unique_ptr<xe::gpu::GraphicsSystem> EmulatorApp::create_graphics_system() {
    Factory<xe::gpu::GraphicsSystem> factory;
    factory.Add<xe::gpu::vulkan::VulkanGraphicsSystem>("vulkan");
    factory.Add<xe::gpu::null::NullGraphicsSystem>("null");
    return factory.Create(cvars::gpu);
}


std::vector<std::unique_ptr<xe::hid::InputDriver>> EmulatorApp::create_input_drivers(xe::ui::Window* window) {
    std::vector<std::unique_ptr<xe::hid::InputDriver>> drivers;
    if (cvars::hid.compare("nop") == 0) {
        drivers.emplace_back(xe::hid::nop::Create(window, xe::app::EmulatorWindow::kZOrderHidInput));
    }
    else {
        Factory<xe::hid::InputDriver, xe::ui::Window *, size_t> factory;
        factory.Add("android", xe::hid::android::Create);

        for (auto &driver: factory.CreateAll(cvars::hid, window,
                                             xe::app::EmulatorWindow::kZOrderHidInput)) {
            if (XSUCCEEDED(driver->Setup())) {
                drivers.emplace_back(std::move(driver));
            }
        }
    }
    if (drivers.empty()) {
        // Fallback to nop if none created.
        drivers.emplace_back(
                xe::hid::nop::Create(window, xe::app::EmulatorWindow::kZOrderHidInput));
    }
    return drivers;
}

void EmulatorApp::emu_thr_main() {
    assert_not_null(emu_thr_event);

    JNIEnv *env;
    g_jvm->AttachCurrentThread(&env, nullptr);

    xe::threading::set_name("Emulator");
    xe::Profiler::ThreadEnter("Emulator");

    // Setup and initialize all subsystems. If we can't do something
    // (unsupported system, memory issues, etc) this will fail early.
    xe::X_STATUS result = emu->Setup(
            emu_window->window(), emu_window->imgui_drawer(), true,
            create_audio_system, create_graphics_system, create_input_drivers);
    if (XFAILED(result)) {
        XELOGE("Failed to setup emulator: {:08X}", result);
        app_context().RequestDeferredQuit();
        return;
    }
    // XenDroid: edge split subsystem creation (audio/graphics/input) out of
    // Setup() into SetupSubsystems(), which the desktop app (xenia_main.cc) runs
    // at first title launch. Bring the subsystems up here, before wiring the
    // presenter and launching the title -- otherwise graphics_system_ stays null
    // and the first Vd* kernel call (e.g. VdSetGraphicsInterruptCallback) crashes.
    if (XFAILED(result = emu->SetupSubsystems())) {
        XELOGE("Failed to setup subsystems: {:08X}", result);
        app_context().RequestDeferredQuit();
        return;
    }
    app_context().CallInUIThread(
            [this]() { emu_window->SetupGraphicsSystemPresenterPainting(); });
    const auto fs = emu->file_system();

    if (cvars::mount_scratch) {
        auto scratch_device = std::make_unique<xe::vfs::HostPathDevice>(
                "\\SCRATCH", emu->storage_root() / "scratch", false);
        if (!scratch_device->Initialize()) {
            XELOGE("Unable to scan scratch path");
        } else {
            if (!fs->RegisterDevice(std::move(scratch_device))) {
                XELOGE("Unable to register scratch path");
            } else {
                fs->RegisterSymbolicLink("scratch:", "\\SCRATCH");
            }
        }
    }

    if (cvars::mount_cache) {
        auto cache0_device = std::make_unique<xe::vfs::HostPathDevice>(
                "\\CACHE0", emu->storage_root() / "cache0", false);
        if (!cache0_device->Initialize()) {
            XELOGE("Unable to scan cache0 path");
        } else {
            if (!fs->RegisterDevice(std::move(cache0_device))) {
                XELOGE("Unable to register cache0 path");
            } else {
                fs->RegisterSymbolicLink("cache0:", "\\CACHE0");
            }
        }

        auto cache1_device = std::make_unique<xe::vfs::HostPathDevice>(
                "\\CACHE1", emu->storage_root() / "cache1", false);
        if (!cache1_device->Initialize()) {
            XELOGE("Unable to scan cache1 path");
        } else {
            if (!fs->RegisterDevice(std::move(cache1_device))) {
                XELOGE("Unable to register cache1 path");
            } else {
                fs->RegisterSymbolicLink("cache1:", "\\CACHE1");
            }
        }

        // Some (older?) games try accessing cache:\ too
        // NOTE: this must be registered _after_ the cache0/cache1 devices, due to
        // substring/start_with logic inside VirtualFileSystem::ResolvePath, else
        // accesses to those devices will go here instead
        auto cache_device = std::make_unique<xe::vfs::HostPathDevice>(
                "\\CACHE", emu->storage_root() / "cache", false);
        if (!cache_device->Initialize()) {
            XELOGE("Unable to scan cache path");
        } else {
            if (!fs->RegisterDevice(std::move(cache_device))) {
                XELOGE("Unable to register cache path");
            } else {
                fs->RegisterSymbolicLink("cache:", "\\CACHE");
            }
        }
    }

    if (cvars::force_mount_devkit) {
        auto devkit_device =
                std::make_unique<xe::vfs::HostPathDevice>("\\DEVKIT", "devkit", false);

        if (!devkit_device->Initialize()) {
            XELOGE("Unable to scan devkit path");
        }

        if (!fs->RegisterDevice(std::move(devkit_device))) {
            XELOGE("Unable to register devkit path");
        }

        fs->RegisterSymbolicLink("DEVKIT:", "\\DEVKIT");
        fs->RegisterSymbolicLink("e:", "\\DEVKIT");
    }

    if (cvars::mount_memory_unit) {
        auto mu_device =
                std::make_unique<xe::vfs::HostPathDevice>("\\MU", "MU", false);

        if (!mu_device->Initialize()) {
            XELOGE("Unable to scan MU path");
        }

        if (!fs->RegisterDevice(std::move(mu_device))) {
            XELOGE("Unable to register MU path");
        }

        fs->RegisterSymbolicLink("MU:", "\\MU");
    }

// Set a debug handler.
// This will respond to debugging requests so we can open the debug UI.
    /*if (cvars::debug) {
        emulator_->processor()->set_debug_listener_request_handler(
                [this](xe::cpu::Processor* processor) {
                    if (debug_window_) {
                        return debug_window_.get();
                    }
                    app_context().CallInUIThreadSynchronous([this]() {
                        debug_window_ = xe::debug::ui::DebugWindow::Create(emulator_.get(),
                                                                           app_context());
                        debug_window_->window()->AddListener(
                                &debug_window_closed_listener_);
                    });
                    // If failed to enqueue the UI thread call, this will just be null.
                    return debug_window_.get();
                });
    }*/
    emu->on_before_shutdown.AddListener([this]() {
        // Mirror desktop xenia_main.cc:753 — null the persistent window's
        // presenter BEFORE the graphics system (and its Presenter) is freed in
        // Shutdown() (emulator.cc:212), so the EVENT_PAINT pump cannot paint
        // through a dangling pointer. Must be synchronous: it has to complete
        // before Shutdown() proceeds past on_before_shutdown() (emulator.cc:192).
        XELOGI("on_before_shutdown[android]: tearing down presenter painting");
        app_context().CallInUIThreadSynchronous(
                [this]() { emu_window->ShutdownGraphicsSystemPresenterPainting(); });
    });
#if 1
    emu->on_launch.AddListener([&](auto title_id, const auto& game_title) {
        XELOGI("on_launch {}",
               game_title.empty() ? "Unknown Title" : std::string(game_title));
        // Mirror desktop xenia_main.cc:722-731 — re-bind the presenter to the
        // persistent ANativeWindow after a new graphics system is built (first
        // boot OR in-process relaunch). Async mirrors the boot path (:448-449);
        // the synchronous on_before_shutdown teardown already left the window
        // presenter-less, so paint ticks before the re-bind are safe no-ops.
        app_context().CallInUIThread(
                [this]() { emu_window->SetupGraphicsSystemPresenterPainting(); });
        app_context().CallInUIThread([this]() { emu_window->UpdateTitle(); });
        emu_thr_event->Set();
    });
#else
    emu->on_launch.AddListener([&](auto title_id, const auto& game_title) {
        /*nlohmann::json json;
        if(std::filesystem::exists(g_uri_info_list_file_path)){
            std::ifstream json_file(g_uri_info_list_file_path);
            json = nlohmann::json::parse(json_file);
            json_file.close();
        }
        if(!game_title.empty()){
            nlohmann::json info;
            info["name"] = game_title;

            json[cvars::target.string()]=info;
        }
        std::ofstream json_file(g_uri_info_list_file_path);
        json_file << json;
        json_file.close();

        emu_thr_event->Set();*/
    });
#endif
    emu->on_shader_storage_initialization.AddListener(
            [this](bool initializing) {
                XELOGI("Shader storage initialization: {}", initializing);
                app_context().CallInUIThread([this, initializing]() {
                    emu_window->SetInitializingShaderStorage(initializing);

                });

            });

    emu->on_patch_apply.AddListener([this]() {
        app_context().CallInUIThread([this]() { emu_window->UpdateTitle(); });
    });

    emu->on_terminate.AddListener([]() {
        XELOGI("Emulator terminated");
    });

    // Enable emulator input now that the emulator is properly loaded.
    app_context().CallInUIThread(
            [this]() { emu_window->OnEmulatorInitialized(); });

    // Grab path from the flag or unnamed argument.
    std::string path;
    if (!cvars::target.empty()) {
        path = cvars::target;
    }

    if (!path.empty()) {
        // Real-path mode (All Files Access): `path` is an absolute host path.
        // LaunchPath does GetFileSignature + MountPath + the correct real-path
        // Launch* overload by extension, and stashes last_launch_path_ for
        // relaunch.
        std::filesystem::path abs = std::filesystem::u8path(path);
        result = app_context().CallInUIThread(
                [this, abs]() { return emu->LaunchPath(abs); });

        if (XFAILED(result)) {
            xe::FatalError(fmt::format("Failed to launch target: {:08X}", result));
            app_context().RequestDeferredQuit();
            return;
        }
    }

    auto xam = emu->kernel_state()->GetKernelModule<xe::kernel::xam::XamModule>(
            "xam.xex");

    if (xam) {
        // XenDroid: edge removed XamModule::LoadLoaderData(); loader_data_ is now populated
        // directly by the xam content/launch handlers (see xam_content.cc).
        if (xam->loader_data().launch_data_present) {
            const std::filesystem::path host_path = xam->loader_data().host_path;
            app_context().CallInUIThread([this, host_path]() {
                return emu_window->RunTitle(host_path);
            });
        }
    }

    // Now, we're going to use this thread to drive events related to emulation.
    /*while (!emu_thr_quit_requested.load(std::memory_order_relaxed)) {
        xe::threading::Wait(emu_thr_event.get(), false);
        emu->WaitUntilExit();
    }*/
    while (!emu_thr_quit_requested.load(std::memory_order_relaxed)) {
        xe::threading::Wait(emu_thr_event.get(), false);
        emu->WaitUntilExit();
    }

    XELOGI("QUIT");
    app_context().QuitFromUIThread();
}

XE_DEFINE_WINDOWED_APP(ax36e,EmulatorApp::create);

namespace ae{

    int boot_type;

    std::string boot_game_path;
    int boot_game_fd;

    ANativeWindow* window;
    int window_width;
    int window_height;
    std::mutex window_mutex;
    // Set once in main_thr; null before boot. Used to marshal surface ops.
    AndroidWindowedAppContext* g_app_context = nullptr;

    std::string game_id;

     std::unique_ptr<xe::ui::WindowedApp> g_windowed_app;
     EmulatorApp* g_windowed_app_ref;

    // n->[n]
    static std::array<xe::ui::VirtualKey,24> key_maps={
            xe::ui::VirtualKey::kXInputPadDpadLeft,
            xe::ui::VirtualKey::kXInputPadDpadUp,
            xe::ui::VirtualKey::kXInputPadDpadRight,
            xe::ui::VirtualKey::kXInputPadDpadDown,
            xe::ui::VirtualKey::kXInputPadA,
            xe::ui::VirtualKey::kXInputPadB,
            xe::ui::VirtualKey::kXInputPadX,
            xe::ui::VirtualKey::kXInputPadY,
            xe::ui::VirtualKey::kXInputPadBack,
            xe::ui::VirtualKey::kXInputPadStart,

            xe::ui::VirtualKey::kXInputPadLShoulder,
            xe::ui::VirtualKey::kXInputPadRShoulder,
            xe::ui::VirtualKey::kXInputPadLThumbPress,
            xe::ui::VirtualKey::kXInputPadRThumbPress,
            xe::ui::VirtualKey::kXInputPadLTrigger,
            xe::ui::VirtualKey::kXInputPadRTrigger,

            xe::ui::VirtualKey::kXInputPadLThumbLeft,
            xe::ui::VirtualKey::kXInputPadLThumbUp,
            xe::ui::VirtualKey::kXInputPadLThumbRight,
            xe::ui::VirtualKey::kXInputPadLThumbDown,
            xe::ui::VirtualKey::kXInputPadRThumbLeft,
            xe::ui::VirtualKey::kXInputPadRThumbUp,
            xe::ui::VirtualKey::kXInputPadRThumbRight,
            xe::ui::VirtualKey::kXInputPadRThumbDown
    };

    void main_thr(){

        std::string tid=[]{
            std::stringstream ss;
            ss<<std::this_thread::get_id();
            return ss.str();
        }();
        LOGW("new thr: %s",tid.c_str());

        prctl(PR_SET_TIMERSLACK,1,0,0,0);

        AndroidWindowedAppContext wnd_ctx;
        wnd_ctx.setup_ui_thr_id(std::this_thread::get_id());
        g_app_context=&wnd_ctx;
        g_windowed_app=xe::ui::GetWindowedAppCreator()(wnd_ctx);
        g_windowed_app_ref=dynamic_cast<EmulatorApp*>(g_windowed_app.get());

        std::vector<char*> args;
        args.push_back(NULL);
        for(auto& i:g_launch_args){
            args.push_back((char*)i.c_str());
        }
        // Real-path mode (All Files Access) feeds an absolute host path to the
        // target cvar parser.
        static std::string boot_target=std::string("--target=")+ae::boot_game_path;
        args.push_back((char*)boot_target.c_str());

        int argc=args.size();
        char** argv=args.data();

        cvar::ParseLaunchArguments(argc, argv, g_windowed_app->GetPositionalOptionsUsage(),
                                   g_windowed_app->GetPositionalOptions());
        xe::InitializeLogging(g_windowed_app->GetName());
        if(g_windowed_app->OnInitialize()){
            wnd_ctx.main_loop();
        }

    }

    void key_event(int key_code,bool pressed,int value){
        static const bool is_android=cvars::hid=="android";
        if(!is_android) return;
        // Every hop can still be null while the detached boot thread builds the emulator,
        // and input arrives in that window: a controller press on the boot splash, or the
        // touch overlay releasing its keys as it leaves composition.
        if(!g_windowed_app_ref || !g_windowed_app_ref->emu) return;
        xe::hid::InputSystem* input_system=g_windowed_app_ref->emu->input_system();
        // driver(0) indexes the vector, so the count must be checked, not the pointer.
        if(!input_system || input_system->driver_count()==0) return;
        auto* driver=reinterpret_cast<xe::hid::android::AndroidInputDriver*>(input_system->driver(0));
        if(!driver) return;
        driver->OnKey(key_code,pressed,value);
    }
    bool is_running(){
        if(!g_windowed_app_ref || !g_windowed_app_ref->emu) return false;
        return !g_windowed_app_ref->emu->is_paused();
    }
    void flush_gpu_caches(){
        if(!g_windowed_app_ref) return;
        xe::Emulator* e = g_windowed_app_ref->emu.get();
        if(!e) return;
        xe::gpu::GraphicsSystem* gs = e->graphics_system();
        if(!gs) return;
        // TODO: re-enable once the persistent VkPipelineCache flush is available
        // (GraphicsSystem::FlushPipelineCache is not present on this baseline).
        // gs->FlushPipelineCache(/*timeout_ms=*/1500);
        (void)gs;
    }
    bool is_paused(){
        if(!g_windowed_app_ref || !g_windowed_app_ref->emu) return false;
        return g_windowed_app_ref->emu->is_paused();
    }
    void pause(){
        // DIRECT call on the calling (Android main) thread. Emulator::Pause() is
        // idempotent and blocks internally on the audio pause_fence_ + the GPU-worker
        // fence; the only requirement is the caller is not the audio worker, the GPU
        // worker, or a guest XThread -- the main thread satisfies all three. Do NOT
        // marshal via CallInUIThread (that is the surface-marshaling path; redundant +
        // would stall the paint pump). g_windowed_app_ref->emu is null until the detached
        // boot thread populates it, so guard exactly like is_paused()/is_running().
        if(g_windowed_app_ref && g_windowed_app_ref->emu)
            g_windowed_app_ref->emu->Pause();
    }
    void resume(){
        if(g_windowed_app_ref && g_windowed_app_ref->emu)
            g_windowed_app_ref->emu->Resume();
    }
    void quit(){
    }

    void init(){
    }

    void surface_detach(){
        ANativeWindow* old = nullptr;
        {
            std::lock_guard<std::mutex> lk(window_mutex);
            old = window;
        }
        // Marshal the teardown to main_thr and BLOCK until it finishes (the
        // fork's CallInUIThread is synchronous via the pump). Only after the
        // GPU drain + vkDestroySurfaceKHR is it safe to release the window.
        if(g_app_context && g_windowed_app_ref){
            // The real AndroidWindow is emu_window->window() (emu_window is an
            // EmulatorWindow, not an AndroidWindow). See the EVENT_PAINT note above.
            AndroidWindow* win = g_windowed_app_ref->emu_window
                ? static_cast<AndroidWindow*>(g_windowed_app_ref->emu_window->window()) : nullptr;
            if(win){
                XELOGI("ae::surface_detach: marshalling DetachSurface to main_thr");
                g_app_context->CallInUIThread([win]{ win->DetachSurface(); });
            }
        }
        {
            std::lock_guard<std::mutex> lk(window_mutex);
            if(window == old) window = nullptr;
        }
        if(old){
            XELOGI("ae::surface_detach: ANativeWindow_release");
            ANativeWindow_release(old);
        }
    }

    void surface_attach(ANativeWindow* w, int width, int height){
        // Defensive: if a surface is somehow still attached (no prior detach),
        // tear it down first so we never leak the old ANativeWindow.
        bool had_old;
        { std::lock_guard<std::mutex> lk(window_mutex); had_old = (window != nullptr); }
        if(had_old){
            XELOGW("ae::surface_attach: previous surface still attached; detaching first");
            surface_detach();
        }
        {
            std::lock_guard<std::mutex> lk(window_mutex);
            window = w;
            window_width = width;
            window_height = height;
        }
        // Pre-boot: just stash the window; main_thr snapshots it during
        // SetupGraphicsSystemPresenterPainting. Post-boot: marshal a recreate
        // (async is fine -- the new swapchain comes up on the next paint tick).
        if(!g_app_context || !g_windowed_app_ref) return;
        // The real AndroidWindow is emu_window->window() (emu_window is an
        // EmulatorWindow, not an AndroidWindow). See the EVENT_PAINT note above.
        AndroidWindow* win = g_windowed_app_ref->emu_window
            ? static_cast<AndroidWindow*>(g_windowed_app_ref->emu_window->window()) : nullptr;
        if(!win) return;
        XELOGI("ae::surface_attach: marshalling UpdateSurface to main_thr");
        g_app_context->CallInUIThread([win]{ win->UpdateSurface(); });
    }

    void surface_resize(int width, int height){
        std::lock_guard<std::mutex> lk(window_mutex);
        window_width = width;
        window_height = height;
        // Size is re-queried from the live ANativeWindow on swapchain recreate,
        // so no separate resize marshal is needed here.
    }

}
