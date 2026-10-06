package xendroid.compose.ui.messagebox

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import xendroid.compose.Emulator
import xendroid.compose.ui.panel.GuestPanelOption
import xendroid.compose.ui.panel.GuestPanelOptions

/**
 * Answers a guest message box (XamShowMessageBoxUI). An in-window Surface, not a Dialog:
 * a Dialog takes window focus and trips the host's focus-loss pause. A guest thread blocks
 * until answered, so [onChoose] must fire for every request (no cancel). [selected] is
 * driven by the host activity because the D-pad arrives as hat axes that never reach a
 * composable.
 */
@Composable
fun GuestMessageBoxPanel(
    request: Emulator.MessageBoxRequest,
    selected: Int,
    onChoose: (Int) -> Unit,
    modifier: Modifier = Modifier,
) {
    val buttons = request.buttons?.takeIf { it.isNotEmpty() } ?: arrayOf("OK")

    BoxWithConstraints(
        modifier
            .fillMaxSize()
            .background(Color.Black.copy(alpha = 0.6f))
            // Swallow taps meant for the game surface underneath.
            .pointerInput(request.id) { awaitPointerEventScope { while (true) awaitPointerEvent() } },
        contentAlignment = Alignment.TopCenter,
    ) {
        val compact = maxHeight < 400.dp
        val outerPadding = if (compact) 8.dp else 24.dp
        val innerPadding = if (compact) 12.dp else 20.dp

        Surface(
            modifier = Modifier
                .widthIn(max = 520.dp)
                .fillMaxWidth()
                // Bounded so a long body scrolls instead of pushing the buttons offscreen.
                .heightIn(max = maxHeight - outerPadding * 2)
                .padding(outerPadding),
            shape = MaterialTheme.shapes.large,
            tonalElevation = 6.dp,
        ) {
            Column(Modifier.padding(innerPadding)) {
                Column(
                    Modifier
                        .weight(1f, fill = false)
                        .verticalScroll(rememberScrollState())
                ) {
                    val title = request.title.orEmpty().trim()
                    if (title.isNotEmpty()) {
                        Text(title, style = MaterialTheme.typography.titleMedium)
                    }
                    val text = request.text.orEmpty().trim()
                    if (text.isNotEmpty()) {
                        Text(
                            text,
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.padding(top = if (compact) 4.dp else 8.dp),
                        )
                    }
                }

                // Full-width, not a Row: guest labels are whole sentences often enough
                // to wrap badly.
                GuestPanelOptions(Modifier.padding(top = if (compact) 8.dp else 16.dp)) {
                    for (i in buttons.indices) {
                        GuestPanelOption(
                            label = buttons[i].ifBlank { "OK" },
                            selected = i == selected,
                            onClick = { onChoose(i) },
                        )
                    }
                }
            }
        }
    }
}
