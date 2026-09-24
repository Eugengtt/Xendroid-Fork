package xendroid.compose.gamepad

import kotlin.math.abs
import kotlin.math.hypot

class GamepadEmitter(private val sink: (Int, Boolean, Int) -> Unit) {

    fun pressDigital(code: Int) = sink(code, true, Kc.VALUE_UNUSED)
    fun releaseDigital(code: Int) = sink(code, false, Kc.VALUE_UNUSED)

    /** One axis -> opposing half-axis codes. d already de-inverted for X;
     *  caller passes the Y component pre-negated (X360 up positive). */
    private fun emitAxisPair(d: Float, negKey: Int, posKey: Int) {
        when {
            d < 0f -> { sink(posKey, false, 0); sink(negKey, true, (d * 32768f).toInt()) }
            d > 0f -> { sink(negKey, false, 0); sink(posKey, true, (d * 32767f).toInt()) }
            else   -> { sink(negKey, false, 0); sink(posKey, false, 0) }
        }
    }

    /** Stick: dxN/dyN are normalized [-1,1] in screen space (dy down-positive).
     *  Circular-clamp BEFORE encode; negate Y for X360 up-positive. */
    fun stick(isLeft: Boolean, dxN: Float, dyN: Float) {
        var x = dxN; var y = dyN
        val len = hypot(x, y)
        if (len > 1f) { x /= len; y /= len }
        val (lx, ly, rx, ry) = if (isLeft)
            intArrayOf(Kc.LTHUMB_LEFT, Kc.LTHUMB_UP, Kc.LTHUMB_RIGHT, Kc.LTHUMB_DOWN)
        else intArrayOf(Kc.RTHUMB_LEFT, Kc.RTHUMB_UP, Kc.RTHUMB_RIGHT, Kc.RTHUMB_DOWN)
        emitAxisPair(x, negKey = lx, posKey = rx)       // X: left=neg, right=pos
        emitAxisPair(-y, negKey = ly, posKey = ry)      // Y negated: up=neg, down=pos
    }

    fun releaseStick(isLeft: Boolean) {
        val codes = if (isLeft) intArrayOf(16, 17, 18, 19) else intArrayOf(20, 21, 22, 23)
        codes.forEach { sink(it, false, 0) }
    }

    /** D-pad radial 8-sector: dx,dy in screen space relative to center; deadzone in px²
     *  passed as normalized len. Returns the set of pressed dpad codes; caller diffs
     *  against previous to emit press/release. */
    /** Legacy VirtualControl 3x3-grid d-pad ("press the arm of the cross"). nx,ny are
     *  normalized offsets from center (-1..1 across the pad). The pad is split into thirds
     *  per axis: outer thirds = that cardinal, center third = neutral on that axis, so corners
     *  give diagonals. Outside the pad box (|n|>1) releases. This matches the old d-pad feel
     *  far better than radial sectoring -- the center is a real dead spot and each arm is a
     *  flat third, not an angle. */
    fun dpadSectors(nx: Float, ny: Float): Set<Int> {
        if (abs(nx) > 1f || abs(ny) > 1f) return emptySet()
        val third = 1f / 3f
        val out = mutableSetOf<Int>()
        if (nx <= -third) out.add(Kc.DPAD_LEFT) else if (nx >= third) out.add(Kc.DPAD_RIGHT)
        if (ny <= -third) out.add(Kc.DPAD_UP) else if (ny >= third) out.add(Kc.DPAD_DOWN)
        return out
    }

    fun applyDpad(prev: Set<Int>, now: Set<Int>) {
        (prev - now).forEach { sink(it, false, Kc.VALUE_UNUSED) }
        (now - prev).forEach { sink(it, true, Kc.VALUE_UNUSED) }
    }
    fun releaseAllDpad() = listOf(0, 1, 2, 3).forEach { sink(it, false, Kc.VALUE_UNUSED) }

    /** Release EVERY gamepad code (digital 0..15, analog half-axes 16..23). Called on
     *  overlay teardown so a control held at dispose time can never stick. */
    fun releaseAll() {
        for (code in 0..15) sink(code, false, Kc.VALUE_UNUSED)
        for (code in 16..23) sink(code, false, 0)
    }
}
