package xendroid.compose.data

import android.view.KeyEvent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import xendroid.compose.data.GameButtons

/** Locks the deterministic transform KeymapStore uses to build its host lookup
 *  (Android keycode -> game KEY_CODE) and its default-filled per-index bindings.
 *  The transform is reproduced here over an explicit "stored prefs" map so it can
 *  be exercised in a plain JVM unit test (the real KeymapStore needs an Android
 *  Context-bound DataStore, which is out of scope for JVM tests). The reproduction
 *  is byte-for-byte the same algorithm as KeymapStore.androidToGameKey/readBindings. */
class KeymapMappingTest {

    /** Mirror of KeymapStore.androidToGameKey over an explicit stored map. */
    private fun androidToGameKey(stored: Map<Int, Int>): Map<Int, Int> {
        val out = HashMap<Int, Int>(GameButtons.ALL.size)
        for (b in GameButtons.ALL) {
            val code = stored[b.index] ?: b.defaultAndroidKey
            if (code != 0) out[code] = b.keyCode
        }
        return out
    }

    /** Mirror of KeymapStore.readBindings over an explicit stored map. */
    private fun readBindings(stored: Map<Int, Int>): Map<Int, Int> {
        val out = HashMap<Int, Int>(GameButtons.ALL.size)
        for (b in GameButtons.ALL) out[b.index] = stored[b.index] ?: b.defaultAndroidKey
        return out
    }

    @Test fun empty_store_yields_default_lookup() =
        assertEquals(GameButtons.DEFAULT_LOOKUP, androidToGameKey(emptyMap()))

    @Test fun rebinding_a_button_overrides_the_default_key() {
        // Bind index 4 (A, default BUTTON_A) to the spacebar instead.
        val out = androidToGameKey(mapOf(4 to KeyEvent.KEYCODE_SPACE))
        assertEquals(4, out[KeyEvent.KEYCODE_SPACE])            // new key drives A
        assertFalse(out.containsKey(KeyEvent.KEYCODE_BUTTON_A)) // old default removed
    }

    @Test fun clearing_a_button_drops_it_from_the_host_lookup() {
        // 0 == cleared/unbound for index 5 (B).
        val out = androidToGameKey(mapOf(5 to 0))
        assertFalse(out.containsKey(KeyEvent.KEYCODE_BUTTON_B))
        assertTrue(out.containsKey(KeyEvent.KEYCODE_BUTTON_A))  // others untouched
    }

    @Test fun rebinding_a_trigger_updates_the_lookup() {
        // Index 14 (Left Trigger) defaults to L2; rebinding it to R1 maps R1 -> game 14.
        val out = androidToGameKey(mapOf(14 to KeyEvent.KEYCODE_BUTTON_R1))
        assertEquals(14, out[KeyEvent.KEYCODE_BUTTON_R1])
    }

    @Test fun readBindings_default_fills_absent_indices() {
        val out = readBindings(mapOf(0 to KeyEvent.KEYCODE_SPACE))
        assertEquals(16, out.size)
        assertEquals(KeyEvent.KEYCODE_SPACE, out[0])                       // overridden
        assertEquals(KeyEvent.KEYCODE_BUTTON_A, out[4])                    // default-filled
        assertEquals(KeyEvent.KEYCODE_BUTTON_L2, out[14])                  // left trigger -> L2
    }
}
