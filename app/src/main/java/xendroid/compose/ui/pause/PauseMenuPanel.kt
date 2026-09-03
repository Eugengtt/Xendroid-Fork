package xendroid.compose.ui.pause

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import xendroid.compose.ui.panel.GuestPanelOption
import xendroid.compose.ui.panel.GuestPanelOptionsRow

/** Option order, mirrored by the host's PanelNav so the controller and the touch UI agree. */
const val PAUSE_OPTION_TOUCH_OVERLAY = 0
const val PAUSE_OPTION_RESUME = 1
const val PAUSE_OPTION_QUIT = 2
const val PAUSE_OPTION_COUNT = 3

/**
 * The in-game menu, styled as the guest prompt panels (keyboard, disc swap, message box) so
 * every in-game overlay looks and navigates alike. An in-window Surface rather than a Dialog:
 * a Dialog takes window focus and trips the host's focus-loss pause. [selected] is driven by
 * the host because the D-pad arrives as hat axes that never reach a composable.
 */
@Composable
fun PauseMenuPanel(
    selected: Int,
    touchOverlayShown: Boolean,
    onToggleTouchOverlay: () -> Unit,
    onResume: () -> Unit,
    onQuit: () -> Unit,
    modifier: Modifier = Modifier,
) {
    BoxWithConstraints(
        modifier
            .fillMaxSize()
            .background(Color.Black.copy(alpha = 0.6f))
            // Swallow taps meant for the game surface underneath.
            .pointerInput(Unit) { awaitPointerEventScope { while (true) awaitPointerEvent() } },
        contentAlignment = Alignment.Center,
    ) {
        val compact = maxHeight < 400.dp
        val outerPadding = if (compact) 8.dp else 24.dp
        val innerPadding = if (compact) 12.dp else 20.dp

        Surface(
            modifier = Modifier
                .widthIn(max = 520.dp)
                .fillMaxWidth()
                .padding(outerPadding),
            shape = MaterialTheme.shapes.large,
            tonalElevation = 6.dp,
        ) {
            Column(Modifier.padding(innerPadding)) {
                Text("Paused", style = MaterialTheme.typography.titleMedium)

                GuestPanelOption(
                    label = if (touchOverlayShown) "Hide controls" else "Show controls",
                    selected = selected == PAUSE_OPTION_TOUCH_OVERLAY,
                    onClick = onToggleTouchOverlay,
                    modifier = Modifier.padding(top = if (compact) 8.dp else 16.dp),
                )
                GuestPanelOptionsRow(Modifier.padding(top = 8.dp)) {
                    GuestPanelOption(
                        label = "Resume",
                        selected = selected == PAUSE_OPTION_RESUME,
                        onClick = onResume,
                        modifier = Modifier.weight(1f),
                    )
                    GuestPanelOption(
                        label = "Quit to library",
                        selected = selected == PAUSE_OPTION_QUIT,
                        onClick = onQuit,
                        modifier = Modifier.weight(1f),
                    )
                }
            }
        }
    }
}
