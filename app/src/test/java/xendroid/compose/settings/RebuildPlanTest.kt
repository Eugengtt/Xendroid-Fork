package xendroid.compose.settings

import org.junit.Assert.assertEquals
import org.junit.Test
import xendroid.compose.settings.RebuildPlan
import xendroid.compose.settings.rebuildPlan

/**
 * Exercises the rebuild-on-flush decision the per-game store relies on (there is no
 * native key-erase, so [GameSettingsRepository.flush] re-emits the WHOLE file from the
 * in-memory override set, deleting it when empty). Verifies the recommended scenario:
 * two overrides -> file has exactly two keys; clear one -> one key; clear last -> delete.
 * This covers the no-native-erase rebuild path without a device/JNI.
 */
class RebuildPlanTest {

    private val a = "Vulkan|vulkan_validation"
    private val b = "Video|widescreen"

    @Test fun twoOverridesWriteExactlyThoseTwoKeys() {
        val plan = rebuildPlan(mapOf(a to "true", b to "false"))
        assertEquals(RebuildPlan.Write(setOf(a, b)), plan)
    }

    @Test fun clearingOneLeavesExactlyOneKey() {
        val plan = rebuildPlan(mapOf(a to "true"))   // b removed
        assertEquals(RebuildPlan.Write(setOf(a)), plan)
    }

    @Test fun clearingTheLastDeletesTheFile() {
        assertEquals(RebuildPlan.Delete, rebuildPlan(emptyMap()))
    }
}
