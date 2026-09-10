package xendroid.compose.ui.library

import xendroid.compose.core.R
import android.content.Context
import android.content.Intent
import android.content.pm.ShortcutInfo
import android.content.pm.ShortcutManager
import android.graphics.BitmapFactory
import android.graphics.drawable.Icon
import android.os.Build
import android.os.SystemClock
import androidx.core.content.getSystemService
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import xendroid.compose.core.EmulatorRuntime
import xendroid.compose.core.ContentPaths
import java.io.File
import xendroid.compose.core.GameMetadataSource
import xendroid.compose.core.ProfileBootstrap
import xendroid.compose.data.Game
import xendroid.compose.data.GameFormat
import xendroid.compose.data.GameLibraryRepository
import xendroid.compose.data.IconCache
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** The launch action emitted on game tap. Resolves once a host Activity registers
 *  for this action; until then the Intent will not resolve (no-op resolveActivity). */
const val ACTION_LAUNCH_GAME = "xendroid.intent.action.xendroid"
const val EXTRA_GAME_URI = "game_uri"
const val EXTRA_DISC_LABELS = "disc_labels"
const val EXTRA_DISC_PATHS = "disc_paths"

/** Minimum time the pull-to-refresh indicator stays up, so a fast warm-cache rescan
 *  doesn't outrun its reveal animation and leave it visually stuck. */
private const val MIN_REFRESH_INDICATOR_MS = 500L

/** Which long-press action triggered title-id resolution (both need the id, then branch). */
enum class GameAction { PER_GAME_SETTINGS, GAME_PATCHES, MANAGE_CONTENT }

/** Async resolution of a game's title id (needed before the per-game settings editor or the
 *  patches screen can open). Driven by the long-press dialog; all formats resolve boot-free. */
sealed interface TitleIdState {
    data object Idle : TitleIdState
    data class Loading(val game: Game, val action: GameAction) : TitleIdState
    data class Resolved(val game: Game, val titleId: String, val action: GameAction) : TitleIdState
    data class Error(val game: Game, val message: String) : TitleIdState
}

class GameLibraryViewModel(
    private val repo: GameLibraryRepository,
    private val iconCache: IconCache,
    private val appContext: Context,
) : ViewModel() {

    private val _state = MutableStateFlow<LibraryUiState>(LibraryUiState.Loading)
    val state: StateFlow<LibraryUiState> = _state.asStateFlow()

    private val _titleId = MutableStateFlow<TitleIdState>(TitleIdState.Idle)
    val titleIdState: StateFlow<TitleIdState> = _titleId.asStateFlow()

    /** True while a scan is running. Drives the swipe-down (pull-to-refresh) indicator
     *  WITHOUT flashing the full-screen spinner over an already-loaded list. */
    private val _isRefreshing = MutableStateFlow(false)
    val isRefreshing: StateFlow<Boolean> = _isRefreshing.asStateFlow()

    init { refresh() }

    fun refresh() {
        if (!EmulatorRuntime.supportsVulkan) { _state.value = LibraryUiState.NoVulkan; return }
        // Keep an existing list visible during a pull-to-refresh (show only the pull
        // indicator); the full-screen spinner is for the first/empty load.
        val wasLoaded = _state.value is LibraryUiState.Loaded
        if (!wasLoaded) _state.value = LibraryUiState.Loading
        _isRefreshing.value = true
        viewModelScope.launch {
            val startMs = SystemClock.elapsedRealtime()
            _state.value = runCatching {
                // ensureLoaded() can sleep + System.loadLibrary on delay-load devices
                // (Adreno 5xx/6xx) -> never on the main thread.
                withContext(Dispatchers.IO) {
                    EmulatorRuntime.ensureLoaded()
                    ProfileBootstrap.ensureDefaultProfile(appContext)
                }
                when (val r = repo.scan()) {
                    GameLibraryRepository.ScanResult.NoFolder -> LibraryUiState.NoFolder
                    GameLibraryRepository.ScanResult.PermissionLost -> LibraryUiState.PermissionLost
                    is GameLibraryRepository.ScanResult.Games -> LibraryUiState.Loaded(r.games)
                }
            }.getOrElse { LibraryUiState.Error(it.message ?: "Failed to load library") }
            // A warm-cache rescan finishes faster than the PullToRefreshBox reveal animation,
            // which leaves the indicator visually stuck; hold it to a floor so it settles before
            // retracting (pull-to-refresh only -- the cold load shows the full-screen spinner).
            if (wasLoaded) {
                val elapsed = SystemClock.elapsedRealtime() - startMs
                if (elapsed < MIN_REFRESH_INDICATOR_MS) delay(MIN_REFRESH_INDICATOR_MS - elapsed)
            }
            _isRefreshing.value = false
        }
    }

    /** Real-path (All Files Access) folder chosen in the built-in browser: persist the
     *  abs path + rescan. */
    fun onRealPathFolderPicked(path: String) {
        viewModelScope.launch {
            repo.saveGameDirPath(path)
            refresh()
        }
    }

    fun requestPerGameSettings(game: Game) = request(game, GameAction.PER_GAME_SETTINGS)
    fun requestGamePatches(game: Game) = request(game, GameAction.GAME_PATCHES)
    fun requestContentManager(game: Game) = request(game, GameAction.MANAGE_CONTENT)

    /** Resolve a game's title id off-main, then the long-press dialog opens the matching screen. */
    private fun request(game: Game, action: GameAction) {
        _titleId.value = TitleIdState.Loading(game, action)
        viewModelScope.launch(Dispatchers.IO) {
            _titleId.value = runCatching {
                EmulatorRuntime.ensureLoaded()
                val tid = repo.readTitleId(appContext, game)
                // 00000000 is the unknown/placeholder title id (no real game carries it).
                if (tid.isNullOrBlank() || tid == "00000000")
                    TitleIdState.Error(game, "Couldn't read this game's title id")
                else TitleIdState.Resolved(game, tid, action)
            }.getOrElse { TitleIdState.Error(game, it.message ?: "Failed to read title id") }
        }
    }

    fun clearTitleIdRequest() { _titleId.value = TitleIdState.Idle }

    /** Build the launch Intent that the host Activity resolves. Caller startActivity()s it. */
    /** Packages [game]'s disc carries that are not installed yet, i.e. the payload a
     *  mandatory-install title (GTA V and friends) needs on the HDD before it will run.
     *  Empty for an ordinary disc, or once its content is installed. Off-main: walks the
     *  disc image. */
    suspend fun uninstalledDiscContent(game: Game): List<GameMetadataSource.DiscContent> =
        withContext(Dispatchers.IO) {
            if (game.format != GameFormat.ISO && game.format != GameFormat.ZAR) {
                return@withContext emptyList()
            }
            EmulatorRuntime.ensureLoaded()
            GameMetadataSource().listDiscContent(game.launchUri).filterNot { item ->
                val titleId = item.titleId ?: return@filterNot false
                File(ContentPaths.contentDir(titleId, item.contentType), item.displayName)
                    .exists()
            }
        }

    fun buildLaunchIntent(game: Game): Intent =
        Intent(ACTION_LAUNCH_GAME).apply {
            setPackage(appContext.packageName)          // self; host is in this app
            putExtra(EXTRA_GAME_URI, game.launchUri)
            // :emu cannot read the library, so the discs travel with the launch.
            val discs = discsOfTitle(game)
            putExtra(EXTRA_DISC_LABELS, discs.map { discLabelOf(it) }.toTypedArray())
            putExtra(EXTRA_DISC_PATHS, discs.map { it.launchUri }.toTypedArray())
        }

    /** Every disc of [game]'s title, including the one being launched: after a swap
     *  the launched disc becomes a swap target again (install disc -> play disc).
     *  Matched on title id, which a set shares; STFS is excluded because updates
     *  and DLC share it too without being discs. */
    fun discsOfTitle(game: Game): List<Game> {
        val titleId = game.titleId?.takeIf { it.isNotBlank() && it != "00000000" }
            ?: return emptyList()
        val all = (_state.value as? LibraryUiState.Loaded)?.games ?: return emptyList()
        return all.filter {
            it.format != GameFormat.STFS &&
                it.titleId?.equals(titleId, ignoreCase = true) == true
        }.sortedWith(compareBy({ if (it.discNumber > 0) it.discNumber else Int.MAX_VALUE },
                                { it.name.lowercase() }))
    }

    /** A set shares one XDBF title, so prefer the header's disc number and fall
     *  back to the file name. */
    fun discLabelOf(game: Game): String = when {
        game.discNumber > 0 && game.discCount > 1 ->
            "Disc ${game.discNumber} of ${game.discCount}"
        game.discNumber > 0 -> "Disc ${game.discNumber}"
        else -> java.io.File(game.launchUri).name
    }

    /** Coil model for a game's icon: the cached PNG File when present, else the
     *  app_icon drawable resource id. Kept here so the View carries no IconCache dep. */
    fun iconFileOrFallback(game: Game): Any {
        val file = game.iconCacheName?.let { iconCache.fileFor(it) }
        return if (file != null && file.exists()) file
        else R.drawable.app_icon
    }

    val isPinShortcutSupported: Boolean
        get() = appContext.getSystemService<ShortcutManager>()
            ?.isRequestPinShortcutSupported == true

    /** True once a host Activity resolves the launch action.
     *  Until then, suppress launch-dependent affordances so we never pin a dead shortcut. */
    val canLaunchGames: Boolean
        get() = Intent(ACTION_LAUNCH_GAME)
            .setPackage(appContext.packageName)
            .resolveActivity(appContext.packageManager) != null

    /** Pin a launcher shortcut. Stable id = game.stableId (uri) to avoid name
     *  collisions (legacy used the display name -> collisions). */
    fun createShortcut(game: Game) {
        if (!isPinShortcutSupported || Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return
        val sm = appContext.getSystemService<ShortcutManager>() ?: return
        val intent = buildLaunchIntent(game).apply { action = Intent.ACTION_VIEW }
        // No resolving host yet; don't pin a shortcut that goes nowhere.
        if (intent.resolveActivity(appContext.packageManager) == null) return
        val icon = game.iconCacheName
            ?.let { iconCache.fileFor(it) }
            ?.takeIf { it.exists() }
            ?.let { BitmapFactory.decodeFile(it.absolutePath) }
            ?.let { Icon.createWithBitmap(it) }
            ?: Icon.createWithResource(appContext, R.drawable.app_icon)
        sm.requestPinShortcut(
            ShortcutInfo.Builder(appContext, game.stableId)
                .setShortLabel(game.name)
                .setIcon(icon)
                .setIntent(intent)
                .build(),
            null
        )
    }
}
