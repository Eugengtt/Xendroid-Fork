package xendroid.compose.gamepad

import androidx.compose.runtime.Immutable

object Kc {
    const val DPAD_LEFT = 0; const val DPAD_UP = 1; const val DPAD_RIGHT = 2; const val DPAD_DOWN = 3
    const val A = 4; const val B = 5; const val X = 6; const val Y = 7
    const val BACK = 8; const val START = 9
    const val SHOULDER_L = 10; const val SHOULDER_R = 11
    const val THUMB_PRESS_L = 12; const val THUMB_PRESS_R = 13
    const val TRIGGER_L = 14; const val TRIGGER_R = 15
    const val LTHUMB_LEFT = 16; const val LTHUMB_UP = 17; const val LTHUMB_RIGHT = 18; const val LTHUMB_DOWN = 19
    const val RTHUMB_LEFT = 20; const val RTHUMB_UP = 21; const val RTHUMB_RIGHT = 22; const val RTHUMB_DOWN = 23
    const val VALUE_UNUSED = -1
}

enum class ControlId {
    DPAD, A, B, X, Y, BACK, START, LB, RB, LT, RT, LS_CLICK, RS_CLICK, LEFT_STICK, RIGHT_STICK
}

@Immutable
sealed interface OnScreenControl {
    val id: ControlId
    val xFraction: Float
    val yFraction: Float
    val scale: Float
    val visible: Boolean
    /** base diameter/size in dp at scale=1 (used for hit-test radius + draw). */
    val baseSizeDp: Float

    data class Button(
        override val id: ControlId, val keyCode: Int, val label: String,
        override val xFraction: Float, override val yFraction: Float,
        override val scale: Float = 1f, override val visible: Boolean = true,
        override val baseSizeDp: Float = 64f,
    ) : OnScreenControl          // also used for LT/RT/LB/RB/LS_CLICK/RS_CLICK (all digital)

    data class Dpad(
        override val id: ControlId,
        override val xFraction: Float, override val yFraction: Float,
        override val scale: Float = 1f, override val visible: Boolean = true,
        override val baseSizeDp: Float = 144f,
    ) : OnScreenControl          // emits codes 0..3 with 8-sector radial logic

    data class AnalogStick(
        override val id: ControlId, val isLeft: Boolean,
        override val xFraction: Float, override val yFraction: Float,
        override val scale: Float = 1f, override val visible: Boolean = true,
        override val baseSizeDp: Float = 132f,
    ) : OnScreenControl          // emits 16..19 (left) or 20..23 (right)
}

fun OnScreenControl.withLayout(
    x: Float = xFraction, y: Float = yFraction, s: Float = scale, vis: Boolean = visible,
): OnScreenControl = when (this) {
    is OnScreenControl.Button -> copy(xFraction = x, yFraction = y, scale = s, visible = vis)
    is OnScreenControl.Dpad -> copy(xFraction = x, yFraction = y, scale = s, visible = vis)
    is OnScreenControl.AnalogStick -> copy(xFraction = x, yFraction = y, scale = s, visible = vis)
}
