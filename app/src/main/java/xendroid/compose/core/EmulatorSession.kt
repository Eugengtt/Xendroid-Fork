package xendroid.compose.core

import android.content.Context
import android.view.Surface
import xendroid.compose.Emulator
import xendroid.emulator.Emulator as BaseEmulator

/**
 * Thin, UI-thread-affine facade over the native xenia singleton. Owns the boot-once invariant;
 * one instance per EmulatorHostActivity (one process / one core).
 *
 * Contract (caller MUST keep order; native enforces nothing):
 *   prepare() -> {setupContext, setupGamePathReal, setupLaunchArgs,
 *                 setupUriInfoListFile}                           [PRE-surface]
 *   attachSurface(s) -> bootOnce()                                [first surface only]
 *   changeSurface(w,h)                                            [surfaceChanged]
 *   attachSurface(s) (+resumeIfPaused) on later surfaceCreated    [NEVER boot again]
 *   detachSurface()                                               [surfaceDestroyed]
 *   flushGpuCaches()                                              [onPause, best-effort]
 *   keyEvent(...)                                                 [hardware input]
 */
class EmulatorSession {

    var booted: Boolean = false
        private set

    /** Throws clearly if used before ensureLoaded(). */
    private val core: Emulator
        get() = EmulatorRuntime.emulator
            ?: error("EmulatorSession used before EmulatorRuntime.ensureLoaded()")

    // ---- PRE-surface setup (all on the main thread, in this exact order) ----

    fun setupContext(ctx: Context) = core.setup_context(ctx)

    /** An ABSOLUTE host path. The String overload routes native -> BOOT_TYPE_WITH_PATH,
     *  which mounts the right real-path device by extension. */
    fun setupGamePathReal(absPath: String) = core.setup_game_path(absPath)

    fun setupLaunchArgs(args: Array<String>) = core.setup_launch_args(args)

    fun setupUriInfoListFile(path: String) = core.setup_uri_info_list_file(path)

    /** Stash/attach the render surface. PRE-boot just stashes; POST-boot async-recreates the
     *  swapchain. Pass null to detach (synchronous GPU drain). */
    fun attachSurface(surface: Surface?) = core.setup_surface(surface)

    fun detachSurface() = core.setup_surface(null)

    fun changeSurface(width: Int, height: Int) = core.change_surface(width, height)

    /** Boot the core EXACTLY ONCE (no-op on later calls). MUST be preceded by a non-null
     *  attachSurface(). Translates the checked BootException. */
    @Throws(RuntimeException::class)
    fun bootOnce() {
        if (booted) return
        booted = true
        try {
            core.boot()
        } catch (e: BaseEmulator.BootException) {
            throw RuntimeException("xenia boot failed", e)
        }
    }

    fun resumeIfPaused() {
        if (core.is_paused()) core.resume()
    }

    fun pause() = core.pause()
    fun resume() = core.resume()

    /** Best-effort GPU cache flush (onPause). Swallows everything; never throws. */
    fun flushGpuCaches() {
        if (!booted) return
        runCatching { EmulatorRuntime.emulator?.flush_gpu_caches() }
    }

    /** Single input sink for ALL controls. value = KEY_VALUE_UNUSED (-1) for digital, or a
     *  signed short magnitude for thumbsticks. Dropped until boot() has run: native key_event
     *  unconditionally derefs g_windowed_app_ref, null until the detached boot thread sets it
     *  (a controller press during the boot splash would crash). */
    fun keyEvent(keyCode: Int, pressed: Boolean, value: Int) {
        if (!booted) return
        core.key_event(keyCode, pressed, value)
    }

    // ---- Debug stats (UI-thread polled; reads native lock-free atomics) ----

    /** Last presented guest-frame interval in ms (0 before first present / after pause). */
    fun lastFrameTimeMs(): Double = if (booted) core.last_frame_time_ms() else 0.0

    /** Instant fps, NOT the average. */
    fun instantFps(): Double = if (booted) core.instant_fps() else 0.0

      fun averageFps(): Double = if (booted) core.average_fps() else 0.0

/** Effective show_debug_overlay (global + per-game override). The override lands on the
 * detached boot thread, so callers must POLL this after boot. */
fun showDebugOverlayEnabled(): Boolean =
    if (booted) core.show_debug_overlay_enabled() else false

fun showTouchOverlayEnabled(): Boolean =
    if (booted) core.show_touch_overlay_enabled() else true

fun setShowTouchOverlay(value: Boolean) {
    if (booted) core.set_show_touch_overlay(value)
}
fun setFpsLimit(value: Int) {
    if (booted) core.set_framerate_limit(value)
}

fun fpsLimit(): Int {
    return if (booted) core.get_framerate_limit() else 60
}
/** FPS limit. */
/** A pending guest text prompt, or null. Holds a dispatch thread until [keyboardSubmit]
 * answers, so every shown panel must be answered. */

    fun keyboardRequest(): Emulator.KeyboardRequest? =
        if (booted) core.keyboard_request() else null

    fun keyboardSubmit(id: Long, accepted: Boolean, text: String) {
        if (booted) core.keyboard_submit(id, accepted, text)
    }

    fun keyboardCancelAll() {
        if (booted) core.keyboard_cancel_all()
    }

    /** A pending guest message box, or null. Blocks a guest thread until [messageBoxSubmit]
     *  answers it, so every shown panel must be answered. */
    fun messageBoxRequest(): Emulator.MessageBoxRequest? =
        if (booted) core.msgbox_request() else null

    fun messageBoxSubmit(id: Long, button: Int) {
        if (booted) core.msgbox_submit(id, button)
    }

    fun messageBoxCancelAll() {
        if (booted) core.msgbox_cancel_all()
    }

    /** Blocks a guest thread until [discSubmit] answers it. */
    fun discRequest(): Emulator.DiscSwapRequest? =
        if (booted) core.disc_request() else null

    fun discSubmit(id: Long, accepted: Boolean, path: String) {
        if (booted) core.disc_submit(id, accepted, path)
    }

    fun discCancelAll() {
        if (booted) core.disc_cancel_all()
    }

    /** Before boot: the guest can ask at any point after. */
    fun discSetKnown(labels: List<String>, paths: List<String>) {
        core.disc_set_known(labels.toTypedArray(), paths.toTypedArray())
    }
}
