package xendroid.compose.ui.panel

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Divider
import androidx.compose.material3.DrawerValue
import androidx.compose.material3.ModalDrawerSheet
import androidx.compose.material3.ModalNavigationDrawer
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberDrawerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import kotlin.math.roundToInt

@Composable
fun GuestSidePanel(
    fpsLimit: Int,
    onFpsLimitChange: (Int) -> Unit,
    performanceOverlayEnabled: Boolean,
    onPerformanceOverlayChange: (Boolean) -> Unit,
    fullscreenStretchEnabled: Boolean,
    onFullscreenStretchChange: (Boolean) -> Unit,
    onExitEmulation: () -> Unit,
    content: @Composable () -> Unit,
) {
    val drawerState = rememberDrawerState(DrawerValue.Closed)

    val pendingFullscreenStretch = remember {
        mutableStateOf<Boolean?>(null)
    }

    val showExitDialog = remember {
        mutableStateOf(false)
    }

    val scrollState = rememberScrollState()

    ModalNavigationDrawer(
        drawerState = drawerState,

        scrimColor = Color.Black.copy(alpha = 0.28f),

        drawerContent = {
            ModalDrawerSheet(
                modifier = Modifier
                    .fillMaxHeight()
                    .fillMaxWidth(0.45f),

                drawerShape = RoundedCornerShape(
                    topEnd = 22.dp,
                    bottomEnd = 22.dp
                ),

                drawerContainerColor = Color.Black.copy(
                    alpha = 0.72f
                ),

                drawerContentColor = Color.White,

                drawerTonalElevation = 0.dp
            ) {
                Column(
                    modifier = Modifier
                        .fillMaxHeight()
                        .verticalScroll(scrollState)
                        .padding(
                            horizontal = 18.dp,
                            vertical = 20.dp
                        )
                ) {

                    // HEADER
                    Column(
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(
                            text = "XenDroid",
                            fontSize = 24.sp,
                            color = Color.White
                        )

                        Spacer(
                            modifier = Modifier.height(3.dp)
                        )

                        Text(
                            text = "Fork by Eugengtt",
                            fontSize = 12.sp,
                            color = Color.White.copy(
                                alpha = 0.60f
                            )
                        )
                    }

                    Spacer(
                        modifier = Modifier.height(16.dp)
                    )

                    Divider(
                        color = Color.White.copy(
                            alpha = 0.14f
                        )
                    )

                    Spacer(
                        modifier = Modifier.height(18.dp)
                    )

                    // FPS LIMIT
                    Text(
                        text = "FPS Limit",
                        fontSize = 14.sp,
                        color = Color.White.copy(
                            alpha = 0.72f
                        )
                    )

                    Spacer(
                        modifier = Modifier.height(3.dp)
                    )

                    Text(
                        text = "$fpsLimit FPS",
                        fontSize = 18.sp,
                        color = Color.White
                    )

                    Spacer(
                        modifier = Modifier.height(6.dp)
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
                            val fpsValues = listOf(
                                30,
                                45,
                                60,
                                90,
                                120
                            )

                            val index = value
                                .roundToInt()
                                .coerceIn(
                                    0,
                                    fpsValues.lastIndex
                                )

                            onFpsLimitChange(
                                fpsValues[index]
                            )
                        },
                        valueRange = 0f..4f,
                        steps = 3,
                        modifier = Modifier.fillMaxWidth()
                    )

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement =
                            Arrangement.SpaceBetween
                    ) {
                        Text(
                            text = "30",
                            fontSize = 11.sp,
                            color = Color.White.copy(
                                alpha = 0.55f
                            )
                        )

                        Text(
                            text = "45",
                            fontSize = 11.sp,
                            color = Color.White.copy(
                                alpha = 0.55f
                            )
                        )

                        Text(
                            text = "60",
                            fontSize = 11.sp,
                            color = Color.White.copy(
                                alpha = 0.55f
                            )
                        )

                        Text(
                            text = "90",
                            fontSize = 11.sp,
                            color = Color.White.copy(
                                alpha = 0.55f
                            )
                        )

                        Text(
                            text = "120",
                            fontSize = 11.sp,
                            color = Color.White.copy(
                                alpha = 0.55f
                            )
                        )
                    }

                    Spacer(
                        modifier = Modifier.height(24.dp)
                    )

                    // PERFORMANCE OVERLAY
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment =
                            Alignment.CenterVertically
                    ) {
                        Column(
                            modifier = Modifier.weight(1f)
                        ) {
                            Text(
                                text = "Performance Overlay",
                                fontSize = 15.sp,
                                color = Color.White
                            )

                            Spacer(
                                modifier = Modifier.height(2.dp)
                            )

                            Text(
                                text =
                                    "FPS and frame-time information",
                                fontSize = 11.sp,
                                color = Color.White.copy(
                                    alpha = 0.52f
                                )
                            )
                        }

                        Switch(
                            checked =
                                performanceOverlayEnabled,
                            onCheckedChange = { enabled ->
                                onPerformanceOverlayChange(
                                    enabled
                                )
                            }
                        )
                    }

                    Spacer(
                        modifier = Modifier.height(22.dp)
                    )

                    // FULLSCREEN STRETCH
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment =
                            Alignment.CenterVertically
                    ) {
                        Column(
                            modifier = Modifier.weight(1f)
                        ) {
                            Text(
                                text = "Fullscreen Stretch",
                                fontSize = 15.sp,
                                color = Color.White
                            )

                            Spacer(
                                modifier = Modifier.height(2.dp)
                            )

                            Text(
                                text =
                                    "Stretch the game image to the display",
                                fontSize = 11.sp,
                                color = Color.White.copy(
                                    alpha = 0.52f
                                )
                            )
                        }

                        Switch(
                            checked =
                                fullscreenStretchEnabled,
                            onCheckedChange = { enabled ->
                                pendingFullscreenStretch.value =
                                    enabled
                            }
                        )
                    }

                    Spacer(
                        modifier = Modifier.height(24.dp)
                    )

                    Divider(
                        color = Color.White.copy(
                            alpha = 0.10f
                        )
                    )

                    Spacer(
                        modifier = Modifier.height(10.dp)
                    )

                    // EXIT EMULATION
                    TextButton(
                        onClick = {
                            showExitDialog.value = true
                        },
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(
                            text = "Exit Emulation",
                            color = Color(0xFF69B34C),
                            fontSize = 14.sp
                        )
                    }

                    Spacer(
                        modifier = Modifier.height(6.dp)
                    )

                    Text(
                        text = "XenDroid Fork",
                        fontSize = 11.sp,
                        color = Color.White.copy(
                            alpha = 0.45f
                        ),
                        modifier = Modifier.align(
                            Alignment.CenterHorizontally
                        )
                    )

                    Spacer(
                        modifier = Modifier.height(8.dp)
                    )
                }
            }
        },

        content = content
    )

    // XBOX-STYLE RESTART DIALOG
    pendingFullscreenStretch.value?.let { requestedValue ->

        Dialog(
            onDismissRequest = {
                pendingFullscreenStretch.value = null
            },
            properties = DialogProperties(
                dismissOnBackPress = true,
                dismissOnClickOutside = true,
                usePlatformDefaultWidth = false
            )
        ) {
            Box(
                modifier = Modifier
                    .fillMaxWidth(0.82f)
                    .background(
                        color = Color.Black.copy(alpha = 0.92f),
                        shape = RoundedCornerShape(8.dp)
                    )
                    .border(
                        width = 1.dp,
                        color = Color.White.copy(alpha = 0.16f),
                        shape = RoundedCornerShape(8.dp)
                    )
                    .padding(20.dp)
            ) {
                Column(
                    modifier = Modifier.fillMaxWidth()
                ) {

                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(3.dp)
                            .background(
                                color = Color(0xFF69B34C),
                                shape = RoundedCornerShape(2.dp)
                            )
                    )

                    Spacer(
                        modifier = Modifier.height(18.dp)
                    )

                    Text(
                        text = "Restart Game?",
                        fontSize = 21.sp,
                        color = Color.White
                    )

                    Spacer(
                        modifier = Modifier.height(8.dp)
                    )

                    Text(
                        text =
                            "This setting will be applied after restarting the game.",
                        fontSize = 13.sp,
                        color = Color.White.copy(
                            alpha = 0.68f
                        )
                    )

                    Spacer(
                        modifier = Modifier.height(22.dp)
                    )

                    Divider(
                        color = Color.White.copy(
                            alpha = 0.12f
                        )
                    )

                    Spacer(
                        modifier = Modifier.height(10.dp)
                    )

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement =
                            Arrangement.End,
                        verticalAlignment =
                            Alignment.CenterVertically
                    ) {

                        TextButton(
                            onClick = {
                                pendingFullscreenStretch.value = null
                            }
                        ) {
                            Text(
                                text = "Cancel",
                                color = Color.White.copy(
                                    alpha = 0.70f
                                ),
                                fontSize = 14.sp
                            )
                        }

                        TextButton(
                            onClick = {
                                pendingFullscreenStretch.value = null

                                onFullscreenStretchChange(
                                    requestedValue
                                )
                            }
                        ) {
                            Text(
                                text = "Restart Game",
                                color = Color(0xFF69B34C),
                                fontSize = 14.sp
                            )
                        }
                    }
                }
            }
        }
    }

    // XBOX-STYLE EXIT DIALOG
    if (showExitDialog.value) {

        Dialog(
            onDismissRequest = {
                showExitDialog.value = false
            },
            properties = DialogProperties(
                dismissOnBackPress = true,
                dismissOnClickOutside = true,
                usePlatformDefaultWidth = false
            )
        ) {
            Box(
                modifier = Modifier
                    .fillMaxWidth(0.82f)
                    .background(
                        color = Color.Black.copy(alpha = 0.92f),
                        shape = RoundedCornerShape(8.dp)
                    )
                    .border(
                        width = 1.dp,
                        color = Color.White.copy(alpha = 0.16f),
                        shape = RoundedCornerShape(8.dp)
                    )
                    .padding(20.dp)
            ) {
                Column(
                    modifier = Modifier.fillMaxWidth()
                ) {

                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(3.dp)
                            .background(
                                color = Color(0xFF69B34C),
                                shape = RoundedCornerShape(2.dp)
                            )
                    )

                    Spacer(
                        modifier = Modifier.height(18.dp)
                    )

                    Text(
                        text = "Exit Emulation?",
                        fontSize = 21.sp,
                        color = Color.White
                    )

                    Spacer(
                        modifier = Modifier.height(8.dp)
                    )

                    Text(
                        text =
                            "Are you sure you want to exit emulation?",
                        fontSize = 13.sp,
                        color = Color.White.copy(
                            alpha = 0.68f
                        )
                    )

                    Spacer(
                        modifier = Modifier.height(22.dp)
                    )

                    Divider(
                        color = Color.White.copy(
                            alpha = 0.12f
                        )
                    )

                    Spacer(
                        modifier = Modifier.height(10.dp)
                    )

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement =
                            Arrangement.End,
                        verticalAlignment =
                            Alignment.CenterVertically
                    ) {

                        TextButton(
                            onClick = {
                                showExitDialog.value = false
                            }
                        ) {
                            Text(
                                text = "Cancel",
                                color = Color.White.copy(
                                    alpha = 0.70f
                                ),
                                fontSize = 14.sp
                            )
                        }

                        TextButton(
                            onClick = {
                                showExitDialog.value = false
                                onExitEmulation()
                            }
                        ) {
                            Text(
                                text = "Yes",
                                color = Color(0xFF69B34C),
                                fontSize = 14.sp
                            )
                        }
                    }
                }
            }
        }
    }
}