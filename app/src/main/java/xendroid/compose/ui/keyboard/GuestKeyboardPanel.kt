package xendroid.compose.ui.keyboard

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import xendroid.compose.Emulator
import xendroid.compose.ui.panel.GuestPanelOption

/**
 * Answers a guest text-entry prompt (XamShowKeyboardUI). An in-window Surface, not a Dialog:
 * a Dialog takes window focus and trips the host's focus-loss pause. A kernel dispatch thread
 * is held until answered, so [onAccept]/[onCancel] must fire for every request. [text] is
 * hoisted so the host can submit it when the controller picks OK; [selected] (0 = OK, 1 =
 * Cancel) is driven by the host because the D-pad arrives as hat axes that never reach a
 * composable.
 *
 * Top-aligned, not centred: with windowSoftInputMode=adjustNothing the window never resizes
 * around the IME, and API 29 devices report no IME insets at all.
 */
@Composable
fun GuestKeyboardPanel(
    request: Emulator.KeyboardRequest,
    text: String,
    onTextChange: (String) -> Unit,
    selected: Int,
    onAccept: (String) -> Unit,
    onCancel: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val maxUnits = request.maxLength.let { if (it <= 0) Int.MAX_VALUE else it }
    val focusRequester = remember(request.id) { FocusRequester() }

    LaunchedEffect(request.id) { focusRequester.requestFocus() }

    BoxWithConstraints(
        modifier
            .fillMaxSize()
            .background(Color.Black.copy(alpha = 0.6f))
            // Swallow taps meant for the game surface underneath.
            .pointerInput(request.id) { awaitPointerEventScope { while (true) awaitPointerEvent() } }
            .imePadding(),
        contentAlignment = Alignment.TopCenter,
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
            Column(
                Modifier
                    .padding(innerPadding)
                    .verticalScroll(rememberScrollState())
            ) {
                val description = request.description.orEmpty()
                if (description.isNotEmpty()) {
                    Text(
                        description,
                        maxLines = if (compact) 1 else 3,
                        overflow = TextOverflow.Ellipsis,
                        style = MaterialTheme.typography.bodyMedium,
                    )
                }
                OutlinedTextField(
                    value = text,
                    onValueChange = { onTextChange(clampToUtf16Units(it, maxUnits)) },
                    singleLine = true,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = if (description.isEmpty()) 0.dp
                                 else if (compact) 8.dp else 16.dp)
                        .heightIn(min = 56.dp)
                        .focusRequester(focusRequester),
                    keyboardOptions = KeyboardOptions(imeAction = ImeAction.Done),
                    keyboardActions = KeyboardActions(onDone = { onAccept(text) }),
                )
                // Side by side: two short labels, and the IME leaves little height.
                Row(
                    Modifier
                        .fillMaxWidth()
                        .padding(top = if (compact) 8.dp else 16.dp),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    GuestPanelOption(
                        label = "OK",
                        selected = selected == 0,
                        onClick = { onAccept(text) },
                        modifier = Modifier.weight(1f),
                    )
                    GuestPanelOption(
                        label = "Cancel",
                        selected = selected == 1,
                        onClick = onCancel,
                        modifier = Modifier.weight(1f),
                    )
                }
            }
        }
    }
}

/** Trims to [maxUnits] UTF-16 code units without splitting a surrogate pair. */
internal fun clampToUtf16Units(text: String, maxUnits: Int): String {
    if (maxUnits <= 0) return ""
    if (text.length <= maxUnits) return text
    var cut = maxUnits
    if (Character.isHighSurrogate(text[cut - 1])) cut--
    return text.substring(0, cut)
}
