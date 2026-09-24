package xendroid.compose.ui.keymap

import android.view.KeyEvent as AndroidKeyEvent
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.clickable
import androidx.compose.foundation.focusable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.input.key.*
import androidx.lifecycle.compose.collectAsStateWithLifecycle

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun KeymapScreen(vm: KeymapViewModel, onBack: () -> Unit) {
    val state by vm.state.collectAsStateWithLifecycle()
    var capturing by remember { mutableStateOf<KeymapRow?>(null) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Key Mapping") },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
                actions = {
                    TextButton(onClick = { vm.onResetDefaults() }) { Text("Reset") }
                },
            )
        }
    ) { padding ->
        LazyColumn(Modifier.fillMaxSize().padding(padding)) {
            items(state.rows, key = { it.button.index }) { row ->
                ListItem(
                    headlineContent = { Text(row.button.label) },
                    supportingContent = { Text(keyLabel(row.boundKey)) },
                    trailingContent = {
                        TextButton(onClick = { vm.onClear(row.button.index) }) { Text("Clear") }
                    },
                    modifier = Modifier.clickable { capturing = row },
                )
                HorizontalDivider()
            }
        }
    }

    capturing?.let { row ->
        KeyCaptureDialog(
            label = row.button.label,
            onKey = { code -> vm.onKeyCaptured(row.button.index, code); capturing = null },
            onDismiss = { capturing = null },
        )
    }
}

@Composable
private fun KeyCaptureDialog(label: String, onKey: (Int) -> Unit, onDismiss: () -> Unit) {
    val focus = remember { FocusRequester() }
    LaunchedEffect(Unit) { focus.requestFocus() }
    BackHandler(enabled = true, onBack = onDismiss)
    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {},
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } },
        title = { Text("Press a key for $label") },
        text = {
            Box(
                Modifier
                    .fillMaxWidth()
                    .focusRequester(focus)
                    .focusable()
                    .onKeyEvent { ev ->
                        if (ev.type == KeyEventType.KeyDown) {
                            onKey(ev.nativeKeyEvent.keyCode); true
                        } else false
                    }
            ) { Text("Waiting for a controller/keyboard button…") }
        },
    )
}

/** Human-readable name for a bound Android keycode (0 = unbound). */
private fun keyLabel(code: Int): String =
    if (code == 0) "(unbound)"
    else AndroidKeyEvent.keyCodeToString(code).removePrefix("KEYCODE_")
