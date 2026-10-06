package xendroid.compose.core

import android.content.Context
import xendroid.compose.settings.ConfigStore
import xendroid.compose.settings.Setting
import xendroid.compose.settings.SettingsSchema

object ProfileBootstrap {
    private const val DEFAULT_GAMERTAG = "XenDroid"

    @Volatile private var ensured = false

    fun ensureDefaultProfile(appContext: Context) {
        if (ensured) return
        val emu = EmulatorRuntime.emulator ?: return
        val root = ContentPaths.contentRoot().absolutePath
        if (emu.list_profiles(root)?.isNotEmpty() == true) { ensured = true; return }
        val xuid = emu.create_profile(root, DEFAULT_GAMERTAG, defaultLanguage(), defaultCountry())
            ?: return
        val h = ConfigStore(appContext).openLive()
        try {
            h.putString("Profiles", "logged_profile_slot_0_xuid", xuid.uppercase())
        } finally {
            h.closeFile()
        }
        ensured = true
    }

    // A cvar's toml section can move, which changes its schema key; fall back
    // rather than hard-cast.
    private fun defaultLanguage() =
        (SettingsSchema.byKey["Console|user_language"] as? Setting.ListChoice)
            ?.default?.toIntOrNull() ?: 1   // en

    private fun defaultCountry() =
        (SettingsSchema.byKey["Console|user_country"] as? Setting.ListChoice)
            ?.default?.toIntOrNull() ?: 103 // United States
}
