package xendroid.compose.ui.library

import xendroid.compose.data.Game
import xendroid.compose.data.GameFormat
import org.junit.Assert.assertTrue
import org.junit.Test
import xendroid.compose.ui.library.LibraryUiState

class LibraryUiStateTest {
    @Test fun loaded_with_games_is_not_empty() {
        val s = LibraryUiState.Loaded(listOf(Game("u", "G", GameFormat.ISO)))
        assertTrue(s.games.isNotEmpty())
    }
    @Test fun loaded_empty_is_distinct_from_no_folder() {
        assertTrue(LibraryUiState.Loaded(emptyList()) is LibraryUiState.Loaded)
        assertTrue(LibraryUiState.NoFolder is LibraryUiState)
    }
}
