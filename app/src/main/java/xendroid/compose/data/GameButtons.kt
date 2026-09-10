package xendroid.compose.data

import android.view.KeyEvent

/** One mappable X360 digital control. [keyCode] is the native VirtualControl KEY_CODE
 *  (0..15) passed to EmulatorSession.keyEvent; [defaultAndroidKey] is the legacy
 *  KeyMapConfig.DEFAULT_KEYMAPPERS binding (0 = unbound). */
data class GameButton(
    val index: Int,
    val keyCode: Int,
    val defaultAndroidKey: Int,
    val label: String,
)

/** The 16 user-mappable digital buttons, index-aligned to the legacy
 *  KeyMapConfig arrays. Codes 16..23 (analog thumb directions) are NOT mappable. */
object GameButtons {
    const val KEY_VALUE_UNUSED = -1

    val ALL: List<GameButton> = listOf(
        GameButton(0,  0,  KeyEvent.KEYCODE_DPAD_LEFT,  "D-Pad Left"),
        GameButton(1,  1,  KeyEvent.KEYCODE_DPAD_UP,    "D-Pad Up"),
        GameButton(2,  2,  KeyEvent.KEYCODE_DPAD_RIGHT, "D-Pad Right"),
        GameButton(3,  3,  KeyEvent.KEYCODE_DPAD_DOWN,  "D-Pad Down"),
        GameButton(4,  4,  KeyEvent.KEYCODE_BUTTON_A,   "A"),          // 96
        GameButton(5,  5,  KeyEvent.KEYCODE_BUTTON_B,   "B"),          // 97
        GameButton(6,  6,  KeyEvent.KEYCODE_BUTTON_X,   "X"),          // 99
        GameButton(7,  7,  KeyEvent.KEYCODE_BUTTON_Y,   "Y"),          // 100
        GameButton(8,  8,  KeyEvent.KEYCODE_BUTTON_SELECT, "Back"),    // 109
        GameButton(9,  9,  KeyEvent.KEYCODE_BUTTON_START,  "Start"),   // 108
        GameButton(10, 10, KeyEvent.KEYCODE_BUTTON_L1,  "Left Shoulder"),  // 102
        GameButton(11, 11, KeyEvent.KEYCODE_BUTTON_R1,  "Right Shoulder"), // 103
        GameButton(12, 12, KeyEvent.KEYCODE_BUTTON_THUMBL, "Left Thumb Press"),  // 106 (L3 stick click)
        GameButton(13, 13, KeyEvent.KEYCODE_BUTTON_THUMBR, "Right Thumb Press"), // 107 (R3 stick click)
        GameButton(14, 14, KeyEvent.KEYCODE_BUTTON_L2,  "Left Trigger"),   // 104 (L2; also via AXIS_LTRIGGER)
        GameButton(15, 15, KeyEvent.KEYCODE_BUTTON_R2,  "Right Trigger"),  // 105 (R2; also via AXIS_RTRIGGER)
    )

    /** Default Android-keycode -> game KEY_CODE map (skips any control left unbound, i.e. key 0). */
    val DEFAULT_LOOKUP: Map<Int, Int> =
        ALL.filter { it.defaultAndroidKey != 0 }
            .associate { it.defaultAndroidKey to it.keyCode }
}
