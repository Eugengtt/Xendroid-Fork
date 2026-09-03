package xendroid.compose

import android.content.Context
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import xendroid.compose.core.GameMetadataSource
import xendroid.compose.data.GameLibraryRepository
import xendroid.compose.data.GameMetadataCache
import xendroid.compose.data.IconCache
import xendroid.compose.data.PreferencesStore
import xendroid.compose.settings.ConfigStore
import xendroid.compose.settings.GameSettingsRepository
import xendroid.compose.settings.GameSettingsViewModel
import xendroid.compose.settings.SettingsRepository
import xendroid.compose.settings.SettingsViewModel
import xendroid.compose.ui.library.GameLibraryViewModel
import xendroid.compose.data.KeymapStore
import xendroid.compose.ui.keymap.KeymapViewModel
import xendroid.compose.ui.compress.GameCompressViewModel
import xendroid.compose.ui.content.ContentManagerViewModel
import xendroid.compose.ui.content.InstallContentViewModel
import xendroid.compose.ui.profile.ProfileManagerViewModel
import xendroid.compose.patches.AssetPatchAssets
import xendroid.compose.patches.GamePatchesViewModel
import xendroid.compose.patches.PatchPaths
import xendroid.compose.patches.PatchStore

/** Manual DI (no Hilt). One instance per process, created lazily in MainActivity
 *  from applicationContext (so it survives config changes / outlives any Activity). */
class AppContainer(context: Context) {
    private val appContext = context.applicationContext

    private val prefs = PreferencesStore(appContext)
    private val keymapStore = KeymapStore(appContext)
    private val metadataSource = GameMetadataSource()
    val iconCache = IconCache(appContext.cacheDir)
    // Per-game extraction-result cache, stored alongside game_icons/ in cacheDir so an
    // OS cache-clear wipes the metadata cache AND the icon files together (stay consistent).
    private val metadataCache = GameMetadataCache(appContext.cacheDir)
    val repository =
        GameLibraryRepository(appContext, prefs, metadataSource, iconCache, metadataCache)

    // ConfigStore is a stateless factory and is safe to share; the SettingsRepository
    // (which owns a single-use ConfigHandle) is built FRESH per ViewModel so one
    // settings VM closing its handle can never pull it out from under another.
    private val configStore = ConfigStore(appContext)

    fun libraryViewModelFactory(): ViewModelProvider.Factory =
        object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                require(modelClass == GameLibraryViewModel::class.java) {
                    "Unknown ViewModel ${modelClass.name}"
                }
                return GameLibraryViewModel(repository, iconCache, appContext) as T
            }
        }

    fun settingsViewModelFactory(): ViewModelProvider.Factory =
        object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                require(modelClass == SettingsViewModel::class.java) { "Unknown ViewModel ${modelClass.name}" }
                return SettingsViewModel(SettingsRepository(configStore)) as T
            }
        }

    /** Per-game settings VM for one title id. Fresh repo per VM (it owns single-use
     *  ConfigHandles), shared stateless configStore — same rule as settingsViewModelFactory. */
    fun gameSettingsViewModelFactory(titleId: String): ViewModelProvider.Factory =
        object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                require(modelClass == GameSettingsViewModel::class.java) { "Unknown ViewModel ${modelClass.name}" }
                return GameSettingsViewModel(GameSettingsRepository(configStore, titleId)) as T
            }
        }

    /** Per-game patches VM for one title id. Stateless asset reads + on-disk toggles; no native. */
    fun gamePatchesViewModelFactory(titleId: String): ViewModelProvider.Factory =
        object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                require(modelClass == GamePatchesViewModel::class.java) { "Unknown ViewModel ${modelClass.name}" }
                return GamePatchesViewModel(
                    titleId,
                    PatchStore(AssetPatchAssets(appContext), PatchPaths.patchesDir()),
                ) as T
            }
        }

    /** Per-game ISO->.zar compressor. The launch path is passed into compress() at call
     *  time so the VM is reusable across games. */
    fun gameCompressViewModelFactory(): ViewModelProvider.Factory =
        object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                require(modelClass == GameCompressViewModel::class.java) {
                    "Unknown ViewModel ${modelClass.name}"
                }
                return GameCompressViewModel(appContext) as T
            }
        }

    /** Per-game content/DLC manager (install + list + delete) for one title id. */
    fun gameContentManagerViewModelFactory(titleId: String): ViewModelProvider.Factory =
        object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                require(modelClass == ContentManagerViewModel::class.java) {
                    "Unknown ViewModel ${modelClass.name}"
                }
                return ContentManagerViewModel(appContext, metadataSource, titleId) as T
            }
        }

    /** Global install-only content entrypoint (no title id, any content type). */
    fun installContentViewModelFactory(): ViewModelProvider.Factory =
        object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                require(modelClass == InstallContentViewModel::class.java) {
                    "Unknown ViewModel ${modelClass.name}"
                }
                return InstallContentViewModel(appContext, metadataSource, prefs) as T
            }
        }

    fun profileManagerViewModelFactory(): ViewModelProvider.Factory =
        object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                require(modelClass == ProfileManagerViewModel::class.java) {
                    "Unknown ViewModel ${modelClass.name}"
                }
                return ProfileManagerViewModel(appContext, configStore) as T
            }
        }

    fun keymapViewModelFactory(): ViewModelProvider.Factory =
        object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                require(modelClass == KeymapViewModel::class.java) {
                    "Unknown ViewModel ${modelClass.name}"
                }
                return KeymapViewModel(keymapStore) as T
            }
        }
}
