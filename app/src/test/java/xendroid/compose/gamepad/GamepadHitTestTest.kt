package xendroid.compose.gamepad

import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.IntSize
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import xendroid.compose.gamepad.ControlId
import xendroid.compose.gamepad.Kc
import xendroid.compose.gamepad.OnScreenControl
import xendroid.compose.gamepad.controlCenterPx
import xendroid.compose.gamepad.controlRadiusPx
import xendroid.compose.gamepad.hitTest

class GamepadHitTestTest {

    // density=1 so dp == px; makes radii easy to reason about.
    private val density = Density(density = 1f, fontScale = 1f)
    private val size = IntSize(1000, 1000)

    @Test
    fun centerPx_isFractionTimesDimension() {
        val c = OnScreenControl.Button(ControlId.A, Kc.A, "A", 0.5f, 0.25f)
        val p = controlCenterPx(c, size)
        assertEquals(500f, p.x)
        assertEquals(250f, p.y)
    }

    @Test
    fun buttonRadius_isHalfBaseSizeTimesScale() {
        val c = OnScreenControl.Button(ControlId.A, Kc.A, "A", 0.5f, 0.5f, scale = 2f, baseSizeDp = 64f)
        // 64dp diameter / 2 = 32, * scale 2 = 64.
        assertEquals(64f, controlRadiusPx(c, density))
    }

    @Test
    fun stickAndDpad_getLargerGrabRadius() {
        val stick = OnScreenControl.AnalogStick(ControlId.LEFT_STICK, true, 0.5f, 0.5f, baseSizeDp = 100f)
        assertEquals(50f * 1.15f, controlRadiusPx(stick, density))
        val dpad = OnScreenControl.Dpad(ControlId.DPAD, 0.5f, 0.5f, baseSizeDp = 100f)
        assertEquals(50f * 1.15f, controlRadiusPx(dpad, density))
    }

    @Test
    fun hitTest_insideRadius_hits_outside_misses() {
        val a = OnScreenControl.Button(ControlId.A, Kc.A, "A", 0.5f, 0.5f, baseSizeDp = 100f) // r=50
        val list = listOf(a)
        // center is (500,500); r=50.
        assertEquals(a, hitTest(list, Offset(530f, 500f), size, density))    // 30px away -> hit
        assertNull(hitTest(list, Offset(600f, 500f), size, density))          // 100px away -> miss
    }

    @Test
    fun hitTest_invisibleControl_neverHits() {
        val a = OnScreenControl.Button(ControlId.A, Kc.A, "A", 0.5f, 0.5f, baseSizeDp = 100f, visible = false)
        assertNull(hitTest(listOf(a), Offset(500f, 500f), size, density))
    }

    @Test
    fun hitTest_returnsFirstMatchInListOrder() {
        val a = OnScreenControl.Button(ControlId.A, Kc.A, "A", 0.5f, 0.5f, baseSizeDp = 200f)
        val b = OnScreenControl.Button(ControlId.B, Kc.B, "B", 0.5f, 0.5f, baseSizeDp = 200f)
        // Both cover (500,500); first in list wins.
        assertEquals(a, hitTest(listOf(a, b), Offset(500f, 500f), size, density))
        assertEquals(b, hitTest(listOf(b, a), Offset(500f, 500f), size, density))
    }
}
