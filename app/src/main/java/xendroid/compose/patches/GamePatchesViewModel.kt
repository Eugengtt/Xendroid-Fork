package xendroid.compose.patches

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

/**
 * Drives the patches screen for one already-resolved title id, reading/writing via [PatchStore].
 * No native involvement, so it never blocks on `EmulatorRuntime`.
 */
class GamePatchesViewModel(
    private val titleId: String,
    private val store: PatchStore,
) : ViewModel() {

    sealed interface UiState {
        data object Loading : UiState
        data class Loaded(val files: List<PatchFile>) : UiState
        data object Empty : UiState
        data class Error(val message: String) : UiState
    }

    private val _state = MutableStateFlow<UiState>(UiState.Loading)
    val state: StateFlow<UiState> = _state.asStateFlow()

    init { reload() }

    private fun reload() {
        viewModelScope.launch(Dispatchers.IO) { _state.value = compute() }
    }

    /** Flip one entry, then re-read effective (on-disk) state so the UI matches what's saved. */
    fun toggle(file: PatchFile, entry: PatchEntry, enabled: Boolean) {
        viewModelScope.launch(Dispatchers.IO) {
            runCatching { store.setEnabled(file.fileName, entry.index, enabled) }
            _state.value = compute()
        }
    }

    private fun compute(): UiState = runCatching {
        val files = store.patchesForTitle(titleId)
        if (files.isEmpty()) UiState.Empty else UiState.Loaded(files)
    }.getOrElse { UiState.Error(it.message ?: "Failed to load patches") }
}
