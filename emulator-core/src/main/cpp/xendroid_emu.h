// SPDX-License-Identifier: WTFPL

#ifndef xendroid_xendroid_EMU_H
#define xendroid_xendroid_EMU_H

#include <cstdint>

#include "xenia/app/emulator_window.h"
#include "xenia/ui/windowed_app_context.h"
#include "xenia/ui/windowed_app.h"
#include "xenia/ui/menu_item.h"
#include "xenia/ui/surface_android.h"

class AndroidWindowedAppContext final : public xe::ui::WindowedAppContext {
public:

    // Pending-work bitmask, guarded by `mutex`. Bit flags (NOT the old 1/2/3
    // equality values) so concurrent producers can't clobber each other.
    uint32_t pending_events = 0;
    static const uint32_t EVENT_EXECUTE_PENDING_FUNCTIONS = 1u << 0;
    static const uint32_t EVENT_QUIT = 1u << 1;
    static const uint32_t EVENT_PAINT = 1u << 2;
    // Generation counters preserving SYNCHRONOUS CallInUIThread semantics:
    // a producer waits until exec_completed >= the ticket it took.
    uint64_t exec_requested = 0;
    uint64_t exec_completed = 0;
    // Set true when main_loop exits (quit). Releases any producer still blocked
    // on a synchronous marshal so it can't hang during teardown.
    bool ui_loop_exited = false;

    pthread_mutex_t mutex;
    pthread_cond_t cond;

    void NotifyUILoopOfPendingFunctions() override;

    void PlatformQuitFromUIThread() override;

    void setup_ui_thr_id(std::thread::id id);

    void request_paint();

    void main_loop();

    AndroidWindowedAppContext();

    ~AndroidWindowedAppContext();

};

class AndroidWindow final : public xe::ui::Window {
public:
    AndroidWindow(xe::ui::WindowedAppContext& app_context, const std::string_view title,
                  uint32_t desired_logical_width, uint32_t desired_logical_height);

protected:
    bool OpenImpl() override;
    void RequestCloseImpl() override;
    std::unique_ptr<xe::ui::Surface> CreateSurfaceImpl(xe::ui::Surface::TypeFlags allowed_types) override;
    void RequestPaintImpl() override;

public:
    void UpdateSurface();   // attach/recreate: OnSurfaceChanged(true)
    void DetachSurface();   // detach only:    OnSurfaceChanged(false)
    void Paint();
};

class android_menu_item final : public xe::ui::MenuItem {
public:
    android_menu_item(Type type, const std::string& text, const std::string& hotkey,
                      std::function<void()> callback);
};


class EmulatorApp final : public xe::ui::WindowedApp {
public:
    static std::unique_ptr<xe::ui::WindowedApp> create(xe::ui::WindowedAppContext& app_context);

    std::unique_ptr<xe::Emulator> emu;
    std::unique_ptr<xe::app::EmulatorWindow> emu_window;
    std::unique_ptr<xe::threading::Event> emu_thr_event;

    std::atomic<bool> emu_thr_quit_requested;
    std::thread emu_thr;

    template <typename T, typename... Args>
    class Factory {
    private:
        struct Creator {
            std::string name;
            std::function<bool()> is_available;
            std::function<std::unique_ptr<T>(Args...)> instantiate;
        };

        std::vector<Creator> creators_;

    public:
        void Add(const std::string_view name, std::function<bool()> is_available,
                 std::function<std::unique_ptr<T>(Args...)> instantiate) {
            creators_.push_back({std::string(name), is_available, instantiate});
        }

        void Add(const std::string_view name,
                 std::function<std::unique_ptr<T>(Args...)> instantiate) {
            auto always_available = []() { return true; };
            Add(name, always_available, instantiate);
        }

        template <typename DT>
        void Add(const std::string_view name) {
            Add(name, DT::IsAvailable, [](Args... args) {
                return std::make_unique<DT>(std::forward<Args>(args)...);
            });
        }

        std::unique_ptr<T> Create(const std::string_view name, Args... args) {
            if (!name.empty() && name != "any") {
                auto it = std::find_if(
                        creators_.cbegin(), creators_.cend(),
                        [&name](const auto& f) { return name.compare(f.name) == 0; });
                if (it != creators_.cend() && (*it).is_available()) {
                    return (*it).instantiate(std::forward<Args>(args)...);
                }
                return nullptr;
            } else {
                for (const auto& creator : creators_) {
                    if (!creator.is_available()) continue;
                    auto instance = creator.instantiate(std::forward<Args>(args)...);
                    if (!instance) continue;
                    return instance;
                }
                return nullptr;
            }
        }

        std::vector<std::unique_ptr<T>> CreateAll(const std::string_view name,
                                                  Args... args) {
            std::vector<std::unique_ptr<T>> instances;
            if (!name.empty() && name != "any") {
                auto it = std::find_if(
                        creators_.cbegin(), creators_.cend(),
                        [&name](const auto& f) { return name.compare(f.name) == 0; });
                if (it != creators_.cend() && (*it).is_available()) {
                    auto instance = (*it).instantiate(std::forward<Args>(args)...);
                    if (instance) {
                        instances.emplace_back(std::move(instance));
                    }
                }
            } else {
                for (const auto& creator : creators_) {
                    if (!creator.is_available()) continue;
                    auto instance = creator.instantiate(std::forward<Args>(args)...);
                    if (instance) {
                        instances.emplace_back(std::move(instance));
                    }
                }
            }
            return instances;
        }
    };

    EmulatorApp(xe::ui::WindowedAppContext& app_context);
    bool OnInitialize() override;
    static std::unique_ptr<xe::apu::AudioSystem> create_audio_system(xe::cpu::Processor* processor);
    static std::unique_ptr<xe::gpu::GraphicsSystem> create_graphics_system();
    static std::vector<std::unique_ptr<xe::hid::InputDriver>> create_input_drivers(xe::ui::Window* window);
    void emu_thr_main();
};
#endif //xendroid_xendroid_EMU_H
