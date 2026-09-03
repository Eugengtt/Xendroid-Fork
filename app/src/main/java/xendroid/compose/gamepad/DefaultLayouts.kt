package xendroid.compose.gamepad

fun defaultLayout(landscape: Boolean): List<OnScreenControl> =
    if (landscape) landscapeDefault() else portraitDefault()

// Hand-tuned on-device layout, baked in; positions are absolute screen fractions
// (grid-snapped on device), scales are the tuned per-control sizes.
private fun landscapeDefault(): List<OnScreenControl> = listOf(
    // Left column.
    OnScreenControl.Button(ControlId.LT, Kc.TRIGGER_L, "LT", 0.0682f, 0.05f),
    OnScreenControl.Button(ControlId.LB, Kc.SHOULDER_L, "LB", 0.0682f, 0.20f),
    OnScreenControl.Dpad(ControlId.DPAD, xFraction = 0.1364f, yFraction = 0.80f, scale = 0.80f),
    OnScreenControl.AnalogStick(
        ControlId.LEFT_STICK, isLeft = true, xFraction = 0.0909f, yFraction = 0.45f, scale = 0.80f
    ),
    OnScreenControl.Button(
        ControlId.LS_CLICK, Kc.THUMB_PRESS_L, "L3", 0.1818f, 0.55f, baseSizeDp = 52f
    ),
    // Right column.
    OnScreenControl.Button(ControlId.RT, Kc.TRIGGER_R, "RT", 0.9318f, 0.05f),
    OnScreenControl.Button(ControlId.RB, Kc.SHOULDER_R, "RB", 0.9318f, 0.20f),
    OnScreenControl.AnalogStick(
        ControlId.RIGHT_STICK, isLeft = false, xFraction = 0.8636f, yFraction = 0.80f, scale = 0.80f
    ),
    OnScreenControl.Button(
        ControlId.RS_CLICK, Kc.THUMB_PRESS_R, "R3", 0.7727f, 0.90f, baseSizeDp = 52f
    ),
    // Face diamond (A bottom, B right, X left, Y top).
    OnScreenControl.Button(ControlId.Y, Kc.Y, "Y", 0.8864f, 0.35f, scale = 0.75f),
    OnScreenControl.Button(ControlId.X, Kc.X, "X", 0.8409f, 0.45f, scale = 0.75f),
    OnScreenControl.Button(ControlId.B, Kc.B, "B", 0.9318f, 0.45f, scale = 0.75f),
    OnScreenControl.Button(ControlId.A, Kc.A, "A", 0.8864f, 0.55f, scale = 0.75f),
    // Back / Start center.
    OnScreenControl.Button(ControlId.BACK, Kc.BACK, "◀", 0.3636f, 0.90f, baseSizeDp = 48f),
    OnScreenControl.Button(ControlId.START, Kc.START, "▶", 0.6364f, 0.90f, baseSizeDp = 48f),
)

private fun portraitDefault(): List<OnScreenControl> {
    // Controls sit in a lower band; t in [0,1] maps top->bottom of that band, thumbs at bottom.
    val yTop = 0.50f
    val ySpan = 0.45f
    fun y(t: Float) = yTop + ySpan * t
    val fx = 0.86f  // face cluster center x
    return listOf(
        // Left column: trigger outside, bumper inside.
        OnScreenControl.Button(ControlId.LT, Kc.TRIGGER_L, "LT", 0.06f, y(0f)),
        OnScreenControl.Button(ControlId.LB, Kc.SHOULDER_L, "LB", 0.15f, y(0f)),
        OnScreenControl.Dpad(ControlId.DPAD, xFraction = 0.13f, yFraction = y(0.40f)),
        OnScreenControl.AnalogStick(
            ControlId.LEFT_STICK, isLeft = true, xFraction = 0.14f, yFraction = y(0.82f)
        ),
        OnScreenControl.Button(
            ControlId.LS_CLICK, Kc.THUMB_PRESS_L, "L3", 0.045f, y(0.82f), baseSizeDp = 52f
        ),
        // Right column.
        OnScreenControl.Button(ControlId.RT, Kc.TRIGGER_R, "RT", 0.94f, y(0f)),
        OnScreenControl.Button(ControlId.RB, Kc.SHOULDER_R, "RB", 0.85f, y(0f)),
        OnScreenControl.AnalogStick(
            ControlId.RIGHT_STICK, isLeft = false, xFraction = fx, yFraction = y(0.40f)
        ),
        OnScreenControl.Button(
            ControlId.RS_CLICK, Kc.THUMB_PRESS_R, "R3", 0.955f, y(0.40f), baseSizeDp = 52f
        ),
        // Face diamond (A bottom, B right, X left, Y top), bottom-right.
        OnScreenControl.Button(ControlId.Y, Kc.Y, "Y", fx, y(0.68f)),
        OnScreenControl.Button(ControlId.X, Kc.X, "X", fx - 0.06f, y(0.82f)),
        OnScreenControl.Button(ControlId.B, Kc.B, "B", fx + 0.06f, y(0.82f)),
        OnScreenControl.Button(ControlId.A, Kc.A, "A", fx, y(0.96f)),
        // Back / Start center.
        OnScreenControl.Button(ControlId.BACK, Kc.BACK, "◀", 0.44f, y(1f), baseSizeDp = 48f),
        OnScreenControl.Button(ControlId.START, Kc.START, "▶", 0.56f, y(1f), baseSizeDp = 48f),
    ).map { it.withLayout(s = 0.75f) }
}
