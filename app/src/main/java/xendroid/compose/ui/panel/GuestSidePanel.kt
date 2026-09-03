package xendroid.compose.ui.panel

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.DrawerValue
import androidx.compose.material3.ModalDrawerSheet
import androidx.compose.material3.ModalNavigationDrawer
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.rememberDrawerState
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlin.math.roundToInt

@Composable
fun GuestSidePanel(
    fpsLimit: Int,
    onFpsLimitChange: (Int) -> Unit,
    performanceOverlayEnabled: Boolean,
    onPerformanceOverlayChange: (Boolean) -> Unit,
    content: @Composable () -> Unit,
) {
    val drawerState = rememberDrawerState(DrawerValue.Closed)

    ModalNavigationDrawer(
        drawerState = drawerState,
        drawerContent = {
            ModalDrawerSheet(
                modifier = Modifier
                    .fillMaxHeight()
                    .fillMaxWidth(0.45f)
            ) {
                Column(
                    modifier = Modifier.padding(16.dp)
                ) {
                    Text(
                        text = "XenDroid",
                        modifier = Modifier.padding(bottom = 16.dp)
                    )

                    // FPS LIMIT
                    Text(
                        text = "FPS Limit: $fpsLimit FPS",
                        modifier = Modifier.padding(bottom = 4.dp)
                    )

                    Slider(
                        value = when (fpsLimit) {
                            30 -> 0f
                            45 -> 1f
                            60 -> 2f
                            90 -> 3f
                            120 -> 4f
                            else -> 2f
                        },
                        onValueChange = { value ->
                            val fpsValues = listOf(30, 45, 60, 90, 120)
                            val index = value
                                .roundToInt()
                                .coerceIn(0, fpsValues.lastIndex)

                            onFpsLimitChange(fpsValues[index])
                        },
                        valueRange = 0f..4f,
                        steps = 3,
                        modifier = Modifier.fillMaxWidth()
                    )

                    Text(
                        text = "30       45       60       90       120"
                    )

                    // PERFORMANCE OVERLAY
                    Text(
                        text = "Performance Overlay",
                        modifier = Modifier.padding(top = 20.dp, bottom = 4.dp)
                    )

                    Switch(
                        checked = performanceOverlayEnabled,
                        onCheckedChange = { enabled ->
                            onPerformanceOverlayChange(enabled)
                        }
                    )
                }
            }
        },
        content = content
    )
}