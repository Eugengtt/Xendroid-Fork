package xendroid.compose.ui.library

import xendroid.compose.data.Game

/** Drives the library screen. NoFolder vs empty Loaded are deliberately distinct
 *  so the UI shows "pick a folder" vs "folder has no games". */
sealed interface LibraryUiState {
    /** No games dir persisted yet -> prompt to pick a folder. */
    data object NoFolder : LibraryUiState
    /** Scanning the chosen folder. */
    data object Loading : LibraryUiState
    /** Scan finished. [games] may be empty (folder has no recognized titles). */
    data class Loaded(val games: List<Game>) : LibraryUiState
    /** A games dir is persisted but it became unreadable (All Files Access grant gone). */
    data object PermissionLost : LibraryUiState
    /** Device has no Vulkan -> emulator cannot run; hard gate. */
    data object NoVulkan : LibraryUiState
    /** Scan/load failed unexpectedly -> show the message + a Retry. */
    data class Error(val message: String) : LibraryUiState
}
