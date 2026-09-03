package xendroid.compose.settings

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import xendroid.compose.settings.ConfigValueShape

/** The native `save_config_entry` re-infers the TOML type from the string shape;
 *  these tests pin the canonical shapes (JNI-free) so the contract can't regress. */
class ConfigValueShapeTest {

    @Test fun bool_shapes() {
        assertEquals("true", ConfigValueShape.bool(true))
        assertEquals("false", ConfigValueShape.bool(false))
    }

    @Test fun int_shapes() {
        assertEquals("5", ConfigValueShape.int(5))
        assertEquals("-3", ConfigValueShape.int(-3))
    }

    @Test fun double_always_carries_exactly_one_dot() {
        assertEquals(1, ConfigValueShape.double(1.5).count { it == '.' })
        assertEquals("2.0", ConfigValueShape.double(2.0))   // never "2"
        assertTrue(ConfigValueShape.double(2.0).contains('.'))
    }

    @Test fun parseBool_round_trips_and_defaults() {
        assertTrue(ConfigValueShape.parseBool("true", false))
        assertFalse(ConfigValueShape.parseBool("false", true))
        assertTrue(ConfigValueShape.parseBool(null, true))       // null -> default
        assertFalse(ConfigValueShape.parseBool("garbage", false)) // garbage -> default
    }

    @Test fun parseInt_round_trips_with_double_tolerance() {
        assertEquals(5, ConfigValueShape.parseInt("5", -1))
        assertEquals(8, ConfigValueShape.parseInt("8.0", -1))   // double round-trip tolerance
        assertEquals(-1, ConfigValueShape.parseInt(null, -1))   // null -> default
        assertEquals(-1, ConfigValueShape.parseInt("xyz", -1))  // garbage -> default
    }
}
