package xendroid.compose.gamepad

import org.junit.Assert.assertEquals
import org.junit.Test
import xendroid.compose.gamepad.snapFrac

class SnapFracTest {

    @Test
    fun snap_roundsToGridSteps() {
        // 10 steps -> grid at 0.0, 0.1, 0.2, ...
        assertEquals(0.5f, snapFrac(0.52f, steps = 10), 1e-6f)
        assertEquals(0.5f, snapFrac(0.48f, steps = 10), 1e-6f)
        assertEquals(0.6f, snapFrac(0.57f, steps = 10), 1e-6f)
    }

    @Test
    fun snap_endpointsStable() {
        assertEquals(0f, snapFrac(0.0f, steps = 10), 1e-6f)
        assertEquals(1f, snapFrac(1.0f, steps = 10), 1e-6f)
    }

    @Test
    fun snap_zeroStepsIsIdentity() {
        assertEquals(0.37f, snapFrac(0.37f, steps = 0), 1e-6f)
    }
}
