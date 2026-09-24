package xendroid.compose.ui.disc

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
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import xendroid.compose.Emulator
import xendroid.compose.ui.panel.GuestPanelOption
import xendroid.compose.ui.panel.GuestPanelOptions

/**
 * Answers a guest disc-swap prompt (XamSwapDisc). An in-window Surface, not a Dialog:
 * a Dialog takes window focus and trips the host's focus-loss pause. A guest thread blocks
 * until answered, so [onChoose]/[onCancel] must fire for every request. The guest ejects
 * before asking, so cancelling leaves the game with no disc. [selected] is driven by the
 * host because the D-pad arrives as hat axes that never reach a composable; Cancel is the
 * LAST option, index discCount.
 */
@Composable
fun DiscSwapPanel(
    request: Emulator.DiscSwapRequest,
    selected: Int,
    onChoose: (String) -> Unit,
    onCancel: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val labels = request.discLabels ?: emptyArray()
    val paths = request.discPaths ?: emptyArray()
    val count = minOf(labels.size, paths.size)

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
                // Bounded so the list scrolls instead of pushing Cancel offscreen.
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
                    Text(
                        if (request.discNumber > 0) "Insert disc ${request.discNumber}"
                        else "Insert disc",
                        style = MaterialTheme.typography.titleMedium,
                    )
                    val message = request.message.orEmpty().trim()
                    if (message.isNotEmpty()) {
                        Text(
                            message,
                            maxLines = if (compact) 2 else 5,
                            overflow = TextOverflow.Ellipsis,
                            style = MaterialTheme.typography.bodyMedium,
                            color = if (request.isError) MaterialTheme.colorScheme.error
                                    else MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.padding(top = if (compact) 4.dp else 8.dp),
                        )
                    }

                    if (count == 0) {
                        Text(
                            "No discs for this title were found in the games folder.",
                            style = MaterialTheme.typography.bodyMedium,
                            modifier = Modifier.padding(top = if (compact) 8.dp else 16.dp),
                        )
                    }
                }

                // Pinned, so a long list cannot hide Cancel.
                GuestPanelOptions(Modifier.padding(top = if (compact) 8.dp else 16.dp)) {
                    for (i in 0 until count) {
                        GuestPanelOption(
                            label = labels[i],
                            selected = i == selected,
                            onClick = { onChoose(paths[i]) },
                        )
                    }
                    GuestPanelOption(
                        label = "Cancel",
                        selected = selected == count,
                        onClick = onCancel,
                    )
                }
            }
        }
    }
}
