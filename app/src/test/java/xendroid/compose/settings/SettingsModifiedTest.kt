package xendroid.compose.settings

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import xendroid.compose.settings.modified

/** Regression guard for the EmulatorSettings.java:591 NPE: a null live/template
 *  value must fall back to the schema default before comparing, never crash, and
 *  never spuriously read as "modified". */
class SettingsModifiedTest {

    @Test fun both_null_fall_to_schema_default_not_modified() {
        assertFalse(
            modified(
                effectiveLive = null,
                effectiveTemplate = null,
                schemaDefault = "false"
            )
        )
    }

    @Test fun live_true_template_false_is_modified() {
        assertTrue(
            modified(
                effectiveLive = "true",
                effectiveTemplate = "false",
                schemaDefault = "false"
            )
        )
    }

    @Test fun live_null_template_matches_schema_default_not_modified() {
        // live null -> schema default "false"; template "false" -> not modified.
        assertFalse(
            modified(
                effectiveLive = null,
                effectiveTemplate = "false",
                schemaDefault = "false"
            )
        )
    }

    @Test fun live_differs_from_template_via_null_fallback_is_modified() {
        // live null -> schema default "false"; template "true" -> modified.
        assertTrue(
            modified(
                effectiveLive = null,
                effectiveTemplate = "true",
                schemaDefault = "false"
            )
        )
    }
}
