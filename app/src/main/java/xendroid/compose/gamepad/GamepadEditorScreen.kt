package xendroid.compose.gamepad

import android.app.Activity
import android.content.Context
import android.content.ContextWrapper
import android.content.pm.ActivityInfo
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import kotlinx.coroutines.launch
import kotlin.math.roundToInt

/** Snap grid: cells along the SHORTER screen edge. The longer edge gets proportionally more
 *  cells of the same pixel pitch, so the cells are square. Higher = smaller (finer) cells. */
const val EDITOR_GRID_STEPS = 20

/** Rounds a fraction to the [EDITOR_GRID_STEPS] grid (a visible, coarse grid so "Snap"
 *  actually aligns controls). steps<=0 is identity. */
fun snapFrac(f: Float, steps: Int = EDITOR_GRID_STEPS): Float {
    if (steps <= 0) return f
    return (f * steps).roundToInt() / steps.toFloat()
}

private tailrec fun Context.findActivity(): Activity? = when (this) {
    is Activity -> this
    is ContextWrapper -> baseContext.findActivity()
    else -> null
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun GamepadEditorScreen(controller: GamepadController, onDone: () -> Unit) {
    val scope = rememberCoroutineScope()
    val persisted by controller.config.collectAsState(initial = GamepadConfigDto())
    var working by remember(persisted) { mutableStateOf(persisted) }
    var landscape by remember { mutableStateOf(true) }
    var selected by remember { mutableStateOf<ControlId?>(null) }
    var snap by remember { mutableStateOf(true) }
    var showGlobals by remember { mutableStateOf(false) }
    var chromeCollapsed by remember { mutableStateOf(false) }
    val base = remember(working, landscape) { controller.controlsFor(working, landscape) }
    // Un-snapped live fraction of the control being dragged. The drag accumulates raw deltas
    // here (NEVER snapped per frame) so it tracks the finger 1:1; snapFrac is applied once on
    // drag-end. Reset explicitly on drag-end. Null when no drag is active.
    var dragRaw by remember { mutableStateOf<Pair<ControlId, Offset>?>(null) }
    var editorSize by remember { mutableStateOf(IntSize.Zero) }
    // Square snap grid: per-axis cell counts from the actual screen size (EDITOR_GRID_STEPS on the
    // shorter edge, same pixel pitch on the longer edge), so the cells are square in any orientation.
    val (stepsX, stepsY) = remember(editorSize) {
        val w = editorSize.width; val h = editorSize.height
        if (w == 0 || h == 0) EDITOR_GRID_STEPS to EDITOR_GRID_STEPS
        else {
            val cell = minOf(w, h).toFloat() / EDITOR_GRID_STEPS
            (w / cell).roundToInt().coerceAtLeast(2) to (h / cell).roundToInt().coerceAtLeast(2)
        }
    }

    val view = LocalView.current
    val activity = remember(view) { view.context.findActivity() }
    // Snapshot the orientation BEFORE the editor forces it (remember{} runs during composition,
    // before the effects below), so on exit we RESTORE it rather than hard-locking landscape --
    // otherwise the library would stay landscape even when the device is held in portrait.
    val enterOrientation = remember {
        activity?.requestedOrientation ?: ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
    }

    // The Landscape/Portrait toggle FORCES the screen orientation, so each layout previews the way
    // it appears in-game. (MainActivity is sensorLandscape-locked, so without this the portrait
    // layout could only be edited stretched across a landscape screen.) MainActivity declares
    // orientation in configChanges, so this rotates WITHOUT recreating the activity (no lost edits).
    LaunchedEffect(landscape, activity) {
        activity?.requestedOrientation = if (landscape)
            ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        else ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
    }

    // Go edge-to-edge immersive while editing, so the overlay matches the fullscreen game exactly
    // (true WYSIWYG) and a control can be placed at the very top/bottom edge. Orientation + bars
    // restored on exit.
    DisposableEffect(Unit) {
        val insets = activity?.window?.let { WindowCompat.getInsetsController(it, view) }
        val prevBehavior = insets?.systemBarsBehavior
        insets?.let {
            it.systemBarsBehavior =
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            it.hide(WindowInsetsCompat.Type.systemBars())
        }
        onDispose {
            activity?.requestedOrientation = enterOrientation
            insets?.let {
                it.show(WindowInsetsCompat.Type.systemBars())
                if (prevBehavior != null) it.systemBarsBehavior = prevBehavior
            }
        }
    }

    fun mutateControls(transform: (List<OnScreenControl>) -> List<OnScreenControl>) {
        val updated = transform(base).toDto()
        working = if (landscape) working.copy(landscape = updated) else working.copy(portrait = updated)
    }
    fun mutateGlobals(transform: (GamepadGlobalsDto) -> GamepadGlobalsDto) {
        working = working.copy(globals = transform(working.globals))
    }

    // Dark backdrop: there's no game behind the overlay in the editor, so the white-outlined
    // controls would be invisible on the light app surface. A solid dark fill makes them visible
    // and previews how they read over a (dark) game frame.
    Box(Modifier.fillMaxSize().background(Color(0xFF15171C)).onSizeChanged { editorSize = it }) {
        // Full-screen, edge-to-edge WYSIWYG overlay: controls can be dragged ANYWHERE on the
        // screen (no top-bar inset). Pass the square-grid cell counts only while Snap is on.
        GamepadOverlay(
            controls = base, opacity = working.globals.opacity, modifier = Modifier.fillMaxSize(),
            onKeyEvent = { _, _, _ -> }, editMode = true,
            gridStepsX = if (snap) stepsX else 0, gridStepsY = if (snap) stepsY else 0,
            selectedId = selected, onSelect = { selected = it },
            onTranslate = { id, dx, dy ->
                // Seed the raw accumulator from the control's CURRENT committed fraction on the
                // first delta of this drag, then advance it by the raw finger delta. Never snap
                // here -- snapping the running value re-rounds sub-half-step deltas back to the
                // same grid cell and freezes the control (the old bug).
                val cur = dragRaw?.takeIf { it.first == id }?.second
                    ?: base.firstOrNull { it.id == id }
                        ?.let { Offset(it.xFraction, it.yFraction) }
                    ?: return@GamepadOverlay
                val nx = (cur.x + dx).coerceIn(0.02f, 0.98f)
                val ny = (cur.y + dy).coerceIn(0.02f, 0.98f)
                dragRaw = id to Offset(nx, ny)
                mutateControls { list ->
                    list.map { if (it.id == id) it.withLayout(x = nx, y = ny) else it }
                }
            },
            onDragEnd = { id ->
                // Commit the grid-snap once, on release, when Snap is on.
                val raw = dragRaw?.takeIf { it.first == id }?.second
                dragRaw = null
                if (snap && raw != null) {
                    // Snap to the nearest IN-BOUNDS square-grid line (per-axis steps). snapFrac can
                    // round to 0.0/1.0 (center on the screen edge, half the control off-screen), so
                    // clamp to [1/steps, (steps-1)/steps] -- real grid lines, fully on-screen.
                    val nx = snapFrac(raw.x, stepsX).coerceIn(1f / stepsX, (stepsX - 1f) / stepsX)
                    val ny = snapFrac(raw.y, stepsY).coerceIn(1f / stepsY, (stepsY - 1f) / stepsY)
                    mutateControls { list ->
                        list.map { if (it.id == id) it.withLayout(x = nx, y = ny) else it }
                    }
                }
            },
            onScale = { id, f -> mutateControls { list ->
                list.map { if (it.id == id) it.withLayout(s = (it.scale * f).coerceIn(0.5f, 3f)) else it } } },
        )

        // Floating, translucent, COLLAPSIBLE toolbar. No opaque top bar -> the top of the
        // screen is free for controls; collapse it to free the bottom too. Taps on the toolbar
        // hit its buttons; taps anywhere else drag a control.
        EditorChrome(
            modifier = Modifier.align(Alignment.BottomCenter).padding(8.dp),
            collapsed = chromeCollapsed,
            onToggleCollapse = { chromeCollapsed = !chromeCollapsed },
            landscape = landscape, onSetLandscape = { landscape = it },
            snap = snap, onToggleSnap = { snap = !snap },
            hasSelection = selected != null,
            // Resize the selected control via an absolute-scale slider (pinch still works too).
            selectedScale = selected?.let { id -> base.firstOrNull { it.id == id }?.scale },
            onScaleSelected = { s ->
                selected?.let { id ->
                    mutateControls { list ->
                        list.map { if (it.id == id) it.withLayout(s = s.coerceIn(0.5f, 3f)) else it }
                    }
                }
            },
            onReset = {
                val def = defaultLayout(landscape).toDto()
                working = if (landscape) working.copy(landscape = def) else working.copy(portrait = def)
            },
            onHideShow = {
                val id = selected ?: return@EditorChrome
                mutateControls { list -> list.map { if (it.id == id) it.withLayout(vis = !it.visible) else it } }
            },
            showGlobals = showGlobals, onToggleGlobals = { showGlobals = !showGlobals },
            globals = working.globals, onMutateGlobals = ::mutateGlobals,
            onCancel = onDone,
            onSave = { scope.launch { controller.save(working); onDone() } },
        )
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun EditorChrome(
    modifier: Modifier,
    collapsed: Boolean,
    onToggleCollapse: () -> Unit,
    landscape: Boolean,
    onSetLandscape: (Boolean) -> Unit,
    snap: Boolean,
    onToggleSnap: () -> Unit,
    hasSelection: Boolean,
    selectedScale: Float?,
    onScaleSelected: (Float) -> Unit,
    onReset: () -> Unit,
    onHideShow: () -> Unit,
    showGlobals: Boolean,
    onToggleGlobals: () -> Unit,
    globals: GamepadGlobalsDto,
    onMutateGlobals: ((GamepadGlobalsDto) -> GamepadGlobalsDto) -> Unit,
    onCancel: () -> Unit,
    onSave: () -> Unit,
) {
    if (collapsed) {
        // Small re-open pill; leaves the whole screen free for placing controls.
        Surface(
            modifier = modifier,
            color = MaterialTheme.colorScheme.surface.copy(alpha = 0.8f),
            shape = RoundedCornerShape(50),
            tonalElevation = 4.dp,
        ) {
            TextButton(onClick = onToggleCollapse,
                modifier = Modifier.padding(horizontal = 4.dp)) { Text("✎  Edit tools") }
        }
        return
    }
    Surface(
        modifier = modifier.widthIn(max = 560.dp),
        color = MaterialTheme.colorScheme.surface.copy(alpha = 0.92f),
        // Pin the content colour: a copy()-d surface colour doesn't resolve to onSurface, so plain
        // Text inside (the Globals labels) would fall back to the default content colour (black)
        // and be illegible on a dark-theme toolbar.
        contentColor = MaterialTheme.colorScheme.onSurface,
        shape = RoundedCornerShape(16.dp),
        tonalElevation = 6.dp,
    ) {
        Column(Modifier.padding(horizontal = 10.dp, vertical = 6.dp)) {
            FlowRow(
                horizontalArrangement = Arrangement.spacedBy(6.dp),
                verticalArrangement = Arrangement.spacedBy(2.dp),
            ) {
                FilterChip(selected = landscape, onClick = { onSetLandscape(true) },
                    label = { Text("Landscape") })
                FilterChip(selected = !landscape, onClick = { onSetLandscape(false) },
                    label = { Text("Portrait") })
                FilterChip(selected = snap, onClick = onToggleSnap, label = { Text("Snap") })
                TextButton(onClick = onReset) { Text("Reset") }
                TextButton(onClick = onHideShow, enabled = hasSelection) { Text("Hide / Show") }
                TextButton(onClick = onToggleGlobals) { Text("Globals") }
                TextButton(onClick = onToggleCollapse) { Text("Collapse ▾") }
            }
            // Size of the selected control (only shown when one is selected). Absolute scale.
            if (selectedScale != null) {
                Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
                    Text("Size", modifier = Modifier.padding(end = 8.dp))
                    Slider(value = selectedScale, valueRange = 0.5f..3f,
                        onValueChange = onScaleSelected, modifier = Modifier.weight(1f))
                    Text("${(selectedScale * 100).roundToInt()}%",
                        modifier = Modifier.padding(start = 8.dp))
                }
            }
            if (showGlobals) GlobalsEditor(globals, onMutateGlobals)
            Row(verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.End,
                modifier = Modifier.fillMaxWidth()) {
                TextButton(onClick = onCancel) { Text("Cancel") }
                Button(onClick = onSave, modifier = Modifier.padding(start = 8.dp)) { Text("Save & Quit") }
            }
        }
    }
}

@Composable
private fun GlobalsEditor(
    globals: GamepadGlobalsDto,
    mutate: ((GamepadGlobalsDto) -> GamepadGlobalsDto) -> Unit,
) {
    Column(Modifier.fillMaxWidth().padding(top = 6.dp)) {
        Text("Whole-pad settings (apply to every control, not the selected one):",
            style = MaterialTheme.typography.labelMedium)
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Enabled", modifier = Modifier.padding(end = 8.dp))
            Switch(checked = globals.enabled, onCheckedChange = { v -> mutate { it.copy(enabled = v) } })
        }
        Text("Opacity: ${(globals.opacity * 100).roundToInt()}%")
        Slider(value = globals.opacity, valueRange = 0.2f..1.0f,
            onValueChange = { v -> mutate { it.copy(opacity = v) } })
        Text("Auto-hide: ${globals.autoHideSeconds.roundToInt()}s (0 = off)")
        Slider(value = globals.autoHideSeconds, valueRange = 0f..20f,
            onValueChange = { v -> mutate { it.copy(autoHideSeconds = v) } })
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Haptics", modifier = Modifier.padding(end = 8.dp))
            Switch(checked = globals.hapticsEnabled,
                onCheckedChange = { v -> mutate { it.copy(hapticsEnabled = v) } })
        }
    }
}
