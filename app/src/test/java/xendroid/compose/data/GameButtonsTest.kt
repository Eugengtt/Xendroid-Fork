package xendroid.compose.data

import android.view.KeyEvent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import xendroid.compose.data.GameButtons

/** Pure-Kotlin checks on the SP3 keymap default table. KeyEvent.KEYCODE_* are
 *  compile-time int constants, inlined into GameButtons, so they resolve to real
 *  values in a plain JVM unit test (no android.jar method dispatch). */
class GameButtonsTest {

    @Test fun has_sixteen_buttons() =
        assertEquals(16, GameButtons.ALL.size)

    @Test fun index_and_keyCode_are_identity_0_to_15() {
        GameButtons.ALL.forEachIndexed { i, b ->
            assertEquals(i, b.index)
            assertEquals(i, b.keyCode)
        }
    }

    @Test fun triggers_default_to_l2_r2() {
        // Left/Right Trigger (game 14/15) bind to the physical triggers KEYCODE_BUTTON_L2/R2.
        assertEquals(KeyEvent.KEYCODE_BUTTON_L2, GameButtons.ALL[14].defaultAndroidKey)
        assertEquals(KeyEvent.KEYCODE_BUTTON_R2, GameButtons.ALL[15].defaultAndroidKey)
    }

    @Test fun default_lookup_binds_all_sixteen() {
        // All 16 controls have a distinct Android keycode now (nothing left unbound).
        assertEquals(16, GameButtons.DEFAULT_LOOKUP.size)
        assertFalse(GameButtons.DEFAULT_LOOKUP.containsKey(0))
    }

    @Test fun default_lookup_maps_android_key_to_game_key_code() {
        // DPAD_LEFT (android) -> game KEY_CODE 0; BUTTON_A -> 4; BUTTON_R1 -> 11.
        assertEquals(0, GameButtons.DEFAULT_LOOKUP[KeyEvent.KEYCODE_DPAD_LEFT])
        assertEquals(4, GameButtons.DEFAULT_LOOKUP[KeyEvent.KEYCODE_BUTTON_A])
        assertEquals(11, GameButtons.DEFAULT_LOOKUP[KeyEvent.KEYCODE_BUTTON_R1])
    }

    @Test fun l2_r2_are_triggers_and_stick_clicks_are_thumb_press() {
        // KEYCODE_BUTTON_L2/R2 (104/105) -> Left/Right Trigger (game 14/15).
        assertEquals(14, GameButtons.DEFAULT_LOOKUP[KeyEvent.KEYCODE_BUTTON_L2])
        assertEquals(15, GameButtons.DEFAULT_LOOKUP[KeyEvent.KEYCODE_BUTTON_R2])
        // Stick clicks (THUMBL/THUMBR, 106/107) -> Left/Right Thumb Press (game 12/13).
        assertEquals(12, GameButtons.DEFAULT_LOOKUP[KeyEvent.KEYCODE_BUTTON_THUMBL])
        assertEquals(13, GameButtons.DEFAULT_LOOKUP[KeyEvent.KEYCODE_BUTTON_THUMBR])
    }

    @Test fun every_button_has_a_label() =
        assertTrue(GameButtons.ALL.all { it.label.isNotBlank() })
}
