package xendroid.compose.settings

/**
 * The row-editing contract the setting rows call into. Extracted so both the global
 * [SettingsViewModel] and the per-game [GameSettingsViewModel] can drive the same
 * [xendroid.compose.ui.settings.SettingRow] composables. The method set is exactly
 * the calls already present in SettingRows.kt.
 */
interface SettingsHost {
    val isCustomDriverSupported: Boolean
    fun currentBool(s: Setting.Bool): Boolean
    fun onBoolChanged(s: Setting.Bool, v: Boolean)
    fun currentInt(s: Setting.IntRange): Int
    fun onIntChanged(s: Setting.IntRange, v: Int)
    fun currentListValue(s: Setting.ListChoice): String
    fun onListChanged(s: Setting.ListChoice, value: String)
    fun currentDriverPath(s: Setting.Action): String
    fun onDriverPathChanged(s: Setting.Action, value: String)
}
