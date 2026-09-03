package xendroid.compose.gamepad

import kotlinx.serialization.json.Json
import org.junit.Assert.assertEquals
import org.junit.Test
import xendroid.compose.gamepad.ControlId
import xendroid.compose.gamepad.ControlLayoutDto
import xendroid.compose.gamepad.GamepadConfigDto
import xendroid.compose.gamepad.GamepadGlobalsDto
import xendroid.compose.gamepad.Kc
import xendroid.compose.gamepad.OnScreenControl
import xendroid.compose.gamepad.OrientationLayoutDto
import xendroid.compose.gamepad.applyTo
import xendroid.compose.gamepad.defaultLayout
import xendroid.compose.gamepad.toDto

class GamepadLayoutSchemaTest {

    private val json = Json { ignoreUnknownKeys = true; encodeDefaults = true }

    @Test
    fun defaultConfig_roundTrips() {
        val original = GamepadConfigDto()
        val text = json.encodeToString(GamepadConfigDto.serializer(), original)
        val decoded = json.decodeFromString<GamepadConfigDto>(text)
        assertEquals(original, decoded)
    }

    @Test
    fun populatedConfig_roundTrips() {
        val original = GamepadConfigDto(
            globals = GamepadGlobalsDto(
                enabled = false,
                opacity = 0.4f,
                autoHideSeconds = 0f,
                hapticsEnabled = true
            ),
            portrait = defaultLayout(false).toDto(),
            landscape = defaultLayout(true).toDto(),
        )
        val text = json.encodeToString(GamepadConfigDto.serializer(), original)
        val decoded = json.decodeFromString<GamepadConfigDto>(text)
        assertEquals(original, decoded)
    }

    @Test
    fun defaultLayout_toDto_applyTo_isIdentity() {
        val landscape = defaultLayout(true)
        assertEquals(landscape, landscape.toDto().applyTo(defaultLayout(true)))
        val portrait = defaultLayout(false)
        assertEquals(portrait, portrait.toDto().applyTo(defaultLayout(false)))
    }

    @Test
    fun defaultLayout_hasFifteenControls() {
        assertEquals(15, defaultLayout(true).size)
        assertEquals(15, defaultLayout(false).size)
    }

    @Test
    fun applyTo_clampsScaleAndMergesOntoCodeFromDefaults() {
        // A persisted entry with an out-of-range scale should be clamped on apply.
        val dto = OrientationLayoutDto(
            listOf(
                ControlLayoutDto(
                    ControlId.A.name,
                    x = 0.5f,
                    y = 0.5f,
                    scale = 9f,
                    visible = false
                )
            )
        )
        val merged = dto.applyTo(defaultLayout(true))
        val a = merged.first { it.id == ControlId.A } as OnScreenControl.Button
        assertEquals(0.5f, a.xFraction)
        assertEquals(0.5f, a.yFraction)
        assertEquals(3f, a.scale)       // clamped to 3.0
        assertEquals(false, a.visible)
        // keyCode/label come from code (defaults), never disk.
        assertEquals(Kc.A, a.keyCode)
        assertEquals("A", a.label)
    }
}
