package xendroid.compose.settings

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import xendroid.compose.core.EmulatorRuntime
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

/**
 * Drives the per-game override editor. Implements [SettingsHost] so it reuses the same
 * SettingRow composables as the global screen; layered on top is the override toggle
 * ([setOverride]) + the inherited-value display. The repo keeps the override set sparse
 * and rebuilds the file on [flush] (no native key-erase exists).
 */
class GameSettingsViewModel(private val repo: GameSettingsRepository) : ViewModel(), SettingsHost {

    val categories: List<SettingsCategory> = SettingsSchema.categories
    override val isCustomDriverSupported get() = repo.isCustomDriverSupported

    /** key -> raw override value (overridden keys only); containsKey == overridden. Value-carrying
     *  so a value-only edit changes the map and the StateFlow emits (a Boolean flag map would be
     *  equals-equal and MutableStateFlow would dedupe -> no recompose). */
    private val _overrides = MutableStateFlow<Map<String, String>>(emptyMap())
    val overrides: StateFlow<Map<String, String>> = _overrides.asStateFlow()

    init { load() }

    /** Off-main load (ensureLoaded() can sleep + System.loadLibrary on delay-load devices). */
    private fun load() {
        viewModelScope.launch(Dispatchers.IO) {
            EmulatorRuntime.ensureLoaded()
            repo.ensureOpen()
            reloadAll()
        }
    }

    private fun reloadAll() {
        _overrides.value = SettingsSchema.allSettings
            .mapNotNull { s -> repo.rawOverride(s)?.let { s.key to it } }
            .toMap()
    }

    private fun refreshKey(s: Setting) {
        _overrides.value = _overrides.value.toMutableMap().apply {
            val raw = repo.rawOverride(s)
            if (raw != null) put(s.key, raw) else remove(s.key)
        }
    }

    fun isOverridden(s: Setting) = repo.isOverridden(s)
    fun inheritedLabel(s: Setting) = repo.inheritedLabel(s)
    fun setOverride(s: Setting, on: Boolean) { repo.setOverride(s, on); refreshKey(s) }

    override fun currentBool(s: Setting.Bool) = repo.boolOf(s)
    override fun onBoolChanged(s: Setting.Bool, v: Boolean) { repo.setBool(s, v); refreshKey(s) }
    override fun currentInt(s: Setting.IntRange) = repo.intOf(s)
    override fun onIntChanged(s: Setting.IntRange, v: Int) { repo.setInt(s, v); refreshKey(s) }
    override fun currentListValue(s: Setting.ListChoice) = repo.listValueOf(s)
    override fun onListChanged(s: Setting.ListChoice, v: String) { repo.setListValue(s, v); refreshKey(s) }
    override fun currentDriverPath(s: Setting.Action) = repo.driverPathOf(s)
    override fun onDriverPathChanged(s: Setting.Action, v: String) { repo.setDriverPath(s, v); refreshKey(s) }

    /** Durable rebuild-on-flush. Call from the screen on lifecycle pause and on dispose. */
    fun flush() { viewModelScope.launch(Dispatchers.IO) { repo.flush() } }

    /** Re-read after a pause; the override set is already in memory so this is cheap. */
    fun onResume() = load()

    override fun onCleared() { viewModelScope.launch(Dispatchers.IO) { repo.flush() } }
}
