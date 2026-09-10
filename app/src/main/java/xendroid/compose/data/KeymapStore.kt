package xendroid.compose.data

import android.content.Context
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.intPreferencesKey
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

/** Persists the 16-button hardware-key mapping + the vibrate toggle in the shared
 *  "xendroid_prefs" DataStore. Keyed by STABLE button index (keymap_0..keymap_15),
 *  NOT the legacy resId-as-string keys. Value = bound Android keycode (0 = cleared). */
class KeymapStore(private val appContext: Context) {

    private fun key(index: Int) = intPreferencesKey("keymap_$index")
    private val vibrateKey = booleanPreferencesKey("enable_vibrator")

    /** button index 0..15 -> bound Android keycode (default-filled where absent). */
    val bindings: Flow<Map<Int, Int>> =
        appContext.dataStore.data.map { prefs -> readBindings(prefs) }

    /** Android keycode -> game KEY_CODE, ready for the host lookup. Unbound (0) dropped. */
    val androidToGameKey: Flow<Map<Int, Int>> =
        appContext.dataStore.data.map { prefs ->
            val out = HashMap<Int, Int>(GameButtons.ALL.size)
            for (b in GameButtons.ALL) {
                val code = prefs[key(b.index)] ?: b.defaultAndroidKey
                if (code != 0) out[code] = b.keyCode
            }
            out
        }

    val vibrateEnabled: Flow<Boolean> =
        appContext.dataStore.data.map { it[vibrateKey] ?: false }

    suspend fun setBinding(index: Int, androidKeyCode: Int) {
        appContext.dataStore.edit { it[key(index)] = androidKeyCode }
    }

    /** Clear a single binding (stores 0 = unbound, matching legacy "Clear"). */
    suspend fun clearBinding(index: Int) = setBinding(index, 0)

    /** Reset all 16 rows to KeyMapConfig.DEFAULT_KEYMAPPERS. */
    suspend fun resetToDefaults() {
        appContext.dataStore.edit { prefs ->
            for (b in GameButtons.ALL) prefs[key(b.index)] = b.defaultAndroidKey
        }
    }

    suspend fun setVibrate(enabled: Boolean) {
        appContext.dataStore.edit { it[vibrateKey] = enabled }
    }

    private fun readBindings(prefs: Preferences): Map<Int, Int> {
        val out = HashMap<Int, Int>(GameButtons.ALL.size)
        for (b in GameButtons.ALL) out[b.index] = prefs[key(b.index)] ?: b.defaultAndroidKey
        return out
    }
}
