package xendroid.compose.settings

import android.content.Context
import android.preference.PreferenceManager
import android.util.Log
import android.view.InputDevice

/** A gamepad or joystick is attached right now (virtual devices excluded, since the
 *  emulator's own injected device advertises gamepad sources). */
fun hasPhysicalController(): Boolean = InputDevice.getDeviceIds().any { id ->
    val device = InputDevice.getDevice(id) ?: return@any false
    val sources = device.sources
    !device.isVirtual &&
        (sources and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
            sources and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK)
}

/**
 * Writes HID|show_touch_overlay once per install, from whether a controller was attached:
 * with one, the overlay starts hidden, without one it starts shown. The cvar's own default
 * cannot express that, and re-deriving it every launch would fight the user's own choice
 * whenever a controller was plugged in or out. Call off the main thread, after the native
 * library is loaded.
 */
fun seedTouchOverlayDefault(context: Context, store: ConfigStore) {
    val prefs = PreferenceManager.getDefaultSharedPreferences(context)
    if (prefs.getBoolean(KEY_SEEDED, false)) return
    val show = !hasPhysicalController()
    runCatching {
        val handle = store.openLive()
        handle.putBool("HID", "show_touch_overlay", show)
        handle.closeFile()
    }.onFailure {
        // Leave the flag unset so the next launch retries rather than silently keeping
        // the cvar default.
        Log.w("TouchOverlay", "seeding show_touch_overlay failed", it)
        return
    }
    prefs.edit().putBoolean(KEY_SEEDED, true).apply()
}

private const val KEY_SEEDED = "touch_overlay_default_seeded"
