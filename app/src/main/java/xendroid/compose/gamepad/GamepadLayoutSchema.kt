package xendroid.compose.gamepad

import kotlinx.serialization.Serializable

@Serializable
data class ControlLayoutDto(
    val id: String,            // ControlId.name
    val x: Float, val y: Float,
    val scale: Float = 1f, val visible: Boolean = true,
)

@Serializable
data class OrientationLayoutDto(val controls: List<ControlLayoutDto> = emptyList())

@Serializable
data class GamepadGlobalsDto(
    val enabled: Boolean = true,
    val opacity: Float = 0.65f,            // ~65% default (spec)
    val autoHideSeconds: Float = 8f,       // 0 disables auto-hide
    val hapticsEnabled: Boolean = false,   // mirrors legacy enable_vibrator default
)

@Serializable
data class GamepadConfigDto(
    val version: Int = 1,
    val globals: GamepadGlobalsDto = GamepadGlobalsDto(),
    val portrait: OrientationLayoutDto = OrientationLayoutDto(),
    val landscape: OrientationLayoutDto = OrientationLayoutDto(),
)

fun OrientationLayoutDto.applyTo(base: List<OnScreenControl>): List<OnScreenControl> {
    val byId = controls.associateBy { it.id }
    return base.map { c ->
        byId[c.id.name]?.let { c.withLayout(it.x, it.y, it.scale.coerceIn(0.5f, 3f), it.visible) } ?: c
    }
}
fun List<OnScreenControl>.toDto() =
    OrientationLayoutDto(map { ControlLayoutDto(it.id.name, it.xFraction, it.yFraction, it.scale, it.visible) })
