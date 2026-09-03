package xendroid.compose.ui.keymap

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import xendroid.compose.data.GameButton
import xendroid.compose.data.GameButtons
import xendroid.compose.data.KeymapStore
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

data class KeymapRow(val button: GameButton, val boundKey: Int)   // boundKey 0 = cleared

data class KeymapUiState(
    val rows: List<KeymapRow> = emptyList(),
    val vibrate: Boolean = false,
)

class KeymapViewModel(private val store: KeymapStore) : ViewModel() {

    val state: StateFlow<KeymapUiState> =
        combine(store.bindings, store.vibrateEnabled) { bindings, vibrate ->
            KeymapUiState(
                rows = GameButtons.ALL.map { b -> KeymapRow(b, bindings[b.index] ?: b.defaultAndroidKey) },
                vibrate = vibrate,
            )
        }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), KeymapUiState())

    fun onKeyCaptured(index: Int, androidKeyCode: Int) =
        viewModelScope.launch { store.setBinding(index, androidKeyCode) }

    fun onClear(index: Int) = viewModelScope.launch { store.clearBinding(index) }

    fun onResetDefaults() = viewModelScope.launch { store.resetToDefaults() }

    fun onVibrateChanged(enabled: Boolean) = viewModelScope.launch { store.setVibrate(enabled) }
}
