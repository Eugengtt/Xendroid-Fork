package xendroid.compose.gamepad

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.math.abs

private data class Ev(val code: Int, val pressed: Boolean, val value: Int)

private class Recorder {
    val events = mutableListOf<Ev>()
    val emitter = GamepadEmitter { c, p, v -> events.add(Ev(c, p, v)) }
    fun clear() = events.clear()
}

class GamepadEmitterTest {

    @Test
    fun digital_pressAndRelease_useUnusedValue() {
        val r = Recorder()
        r.emitter.pressDigital(Kc.A)
        assertEquals(Ev(4, true, -1), r.events.last())
        r.emitter.releaseDigital(Kc.A)
        assertEquals(Ev(4, false, -1), r.events.last())
    }

    @Test
    fun stick_fullRight_emitsPositiveXandReleasesY() {
        val r = Recorder()
        r.emitter.stick(isLeft = true, dxN = 1f, dyN = 0f)
        // X positive: LEFT(16) released, RIGHT(18) pressed with +32767.
        assertTrue(Ev(16, false, 0) in r.events)
        assertTrue(Ev(18, true, 32767) in r.events)
        // Y centered: both UP(17)/DOWN(19) released at 0.
        assertTrue(Ev(17, false, 0) in r.events)
        assertTrue(Ev(19, false, 0) in r.events)
    }

    @Test
    fun stick_fingerDown_emitsUpWithNegativeMagnitude_legacyParity() {
        // Pinned to legacy Joystick.send_key_event: finger below center (dy=+1)
        // emits UP (17) with negative magnitude (-32768), DOWN (19) released.
        val r = Recorder()
        r.emitter.stick(isLeft = true, dxN = 0f, dyN = 1f)
        assertTrue(Ev(19, false, 0) in r.events)
        assertTrue(Ev(17, true, -32768) in r.events)
        // X centered.
        assertTrue(Ev(16, false, 0) in r.events)
        assertTrue(Ev(18, false, 0) in r.events)
    }

    @Test
    fun stick_diagonal_circularClampsMagnitudes() {
        val r = Recorder()
        r.emitter.stick(isLeft = true, dxN = 1f, dyN = 1f)
        // After circular clamp each component ~0.7071 -> ~±23170, not ±32767.
        val rightX = r.events.first { it.code == 18 }.value          // +0.7071*32767
        val upY = r.events.first { it.code == 17 }.value             // -0.7071*32768
        assertTrue("expected ~23170 got $rightX", abs(rightX - 23170) <= 2)
        assertTrue("expected ~-23170 got $upY", abs(upY + 23170) <= 2)
    }

    @Test
    fun stick_right_emitsTwentyToTwentyThreeWhenNotLeft() {
        val r = Recorder()
        r.emitter.stick(isLeft = false, dxN = 1f, dyN = 0f)
        assertTrue(Ev(20, false, 0) in r.events)        // RTHUMB_LEFT released
        assertTrue(Ev(22, true, 32767) in r.events)     // RTHUMB_RIGHT pressed
    }

    @Test
    fun releaseStick_releasesAllFourOwnedCodes() {
        val r = Recorder()
        r.emitter.releaseStick(isLeft = true)
        assertEquals(
            listOf(Ev(16, false, 0), Ev(17, false, 0), Ev(18, false, 0), Ev(19, false, 0)),
            r.events,
        )
        r.clear()
        r.emitter.releaseStick(isLeft = false)
        assertEquals(
            listOf(Ev(20, false, 0), Ev(21, false, 0), Ev(22, false, 0), Ev(23, false, 0)),
            r.events,
        )
    }

    // nx,ny normalized to [-1,1] across the pad; 3x3 grid (outer third = arm, center neutral).
    @Test
    fun dpadSectors_corner_returnsRightAndUp() {
        val r = Recorder()
        assertEquals(setOf(Kc.DPAD_RIGHT, Kc.DPAD_UP), r.emitter.dpadSectors(nx = 0.5f, ny = -0.5f))
    }

    @Test
    fun dpadSectors_cardinalArms() {
        val r = Recorder()
        assertEquals(setOf(Kc.DPAD_RIGHT), r.emitter.dpadSectors(0.5f, 0f))
        assertEquals(setOf(Kc.DPAD_UP), r.emitter.dpadSectors(0f, -0.5f))   // up = -ny
        assertEquals(setOf(Kc.DPAD_LEFT), r.emitter.dpadSectors(-0.5f, 0f))
        assertEquals(setOf(Kc.DPAD_DOWN), r.emitter.dpadSectors(0f, 0.5f))  // down = +ny
    }

    @Test
    fun dpadSectors_centerThirdIsNeutral() {
        val r = Recorder()
        // both axes within the center third (|n| < 1/3) -> no direction.
        assertEquals(emptySet<Int>(), r.emitter.dpadSectors(nx = 0.2f, ny = 0.2f))
    }

    @Test
    fun dpadSectors_outsidePadReleases() {
        val r = Recorder()
        // finger dragged off the pad box (|n| > 1) -> neutral (legacy rect.contains miss).
        assertEquals(emptySet<Int>(), r.emitter.dpadSectors(nx = 1.5f, ny = 0f))
    }

    @Test
    fun applyDpad_emitsPressOnNewAndReleaseOnGone() {
        val r = Recorder()
        r.emitter.applyDpad(prev = setOf(Kc.DPAD_RIGHT), now = setOf(Kc.DPAD_UP))
        // RIGHT released, UP pressed.
        assertTrue(Ev(Kc.DPAD_RIGHT, false, -1) in r.events)
        assertTrue(Ev(Kc.DPAD_UP, true, -1) in r.events)
    }

    @Test
    fun releaseAllDpad_releasesZeroToThree() {
        val r = Recorder()
        r.emitter.releaseAllDpad()
        assertEquals(
            listOf(Ev(0, false, -1), Ev(1, false, -1), Ev(2, false, -1), Ev(3, false, -1)),
            r.events,
        )
    }
}
