package xendroid.compose.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import xendroid.compose.data.GameFormat

class GameFormatTest {
    @Test fun iso_detected_by_extension() =
        assertEquals(GameFormat.ISO, GameFormat.fromFileName("Halo 3.iso"))
    @Test fun zar_detected_by_extension() =
        assertEquals(GameFormat.ZAR, GameFormat.fromFileName("game.zar"))
    @Test fun extensionless_is_god() =
        assertEquals(GameFormat.GOD, GameFormat.fromFileName("5841125F"))
    @Test fun dotted_non_iso_zar_is_unknown() =
        assertNull(GameFormat.fromFileName("readme.txt"))
    @Test fun iso_is_case_insensitive() =
        assertEquals(GameFormat.ISO, GameFormat.fromFileName("Game.ISO"))

    // launch-name derivation matches legacy: strip the 4-char extension for ISO/ZAR.
    @Test fun displayName_strips_iso_extension() =
        assertEquals("Halo 3", GameFormat.ISO.displayNameFor("Halo 3.iso"))
    @Test fun displayName_god_keeps_filename() =
        assertEquals("5841125F", GameFormat.GOD.displayNameFor("5841125F"))
}
