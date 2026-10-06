package xendroid.compose.gamepad

import android.content.Context
import android.os.SystemClock
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import java.util.concurrent.atomic.AtomicLong
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.first

class GamepadController(appContext: Context) {
    private val store = GamepadLayoutStore(appContext)
    val config: Flow<GamepadConfigDto> = store.config
    suspend fun save(cfg: GamepadConfigDto) = store.save(cfg)

    /** Runtime controls for an orientation = defaults merged with persisted layout. */
    fun controlsFor(cfg: GamepadConfigDto, landscape: Boolean): List<OnScreenControl> {
        val base = defaultLayout(landscape)
        val dto = if (landscape) cfg.landscape else cfg.portrait
        return dto.applyTo(base)
    }
}

@Composable
fun rememberAutoHide(autoHideSeconds: Float): Pair<Boolean, () -> Unit> {
    var visible by remember { mutableStateOf(true) }
    // Pokes arrive per pointer event (~120Hz in a drag): a per-event snapshot write recomposed
    // the whole host scope and, as a LaunchedEffect key, relaunched the timer per event.
    // Hence the plain timestamp. (visible = true while already true is an equal-value no-op.)
    val lastPokeMs = remember { AtomicLong(SystemClock.uptimeMillis()) }
    val poke = remember {
        {
            lastPokeMs.set(SystemClock.uptimeMillis())
            visible = true
        }
    }
    if (autoHideSeconds > 0f) {
        LaunchedEffect(autoHideSeconds) {
            lastPokeMs.set(SystemClock.uptimeMillis())
            val timeoutMs = (autoHideSeconds * 1000).toLong()
            while (true) {
                val remaining = lastPokeMs.get() + timeoutMs - SystemClock.uptimeMillis()
                if (remaining > 0) {
                    delay(remaining)
                } else {
                    visible = false   // overlay then animates alpha to 0 over 0.5s
                    snapshotFlow { visible }.first { it }   // park until the next poke wakes us
                }
            }
        }
    }
    return visible to poke
}
