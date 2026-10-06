package xendroid.compose.gamepad

import android.content.Context
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import kotlinx.serialization.json.Json

// Distinct DataStore name so :emu and :main never share a single-writer file handle.
private val Context.gamepadStore by preferencesDataStore(name = "xendroid_gamepad")

class GamepadLayoutStore(private val appContext: Context) {
    private val json = Json { ignoreUnknownKeys = true; encodeDefaults = true }
    private val key = stringPreferencesKey("gamepad_config_v1")

    val config: Flow<GamepadConfigDto> = appContext.gamepadStore.data.map { prefs ->
        prefs[key]?.let { runCatching { json.decodeFromString<GamepadConfigDto>(it) }.getOrNull() }
            ?: GamepadConfigDto()
    }

    suspend fun save(cfg: GamepadConfigDto) {
        val text = json.encodeToString(GamepadConfigDto.serializer(), cfg)
        appContext.gamepadStore.edit { it[key] = text }
    }
}
