package xendroid.compose.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

// Xbox green brand palette (replaces Material's default purple for primary/secondary/tertiary).
// Light uses the deep Xbox green (#107C10) for white-on-green buttons; dark uses a brighter green
// that reads on dark surfaces. Surfaces/background/error stay Material defaults.
private val DarkColors = darkColorScheme(
    primary = Color(0xFF6FD75F),
    onPrimary = Color(0xFF00390A),
    primaryContainer = Color(0xFF0B5D12),
    onPrimaryContainer = Color(0xFFA6F398),
    inversePrimary = Color(0xFF107C10),
    secondary = Color(0xFFB9CCB3),
    onSecondary = Color(0xFF253423),
    secondaryContainer = Color(0xFF3B4B38),
    onSecondaryContainer = Color(0xFFD5E8CF),
    tertiary = Color(0xFFA0CFD4),
    onTertiary = Color(0xFF00363B),
    tertiaryContainer = Color(0xFF1E4D52),
    onTertiaryContainer = Color(0xFFBCEBF0),
)
private val LightColors = lightColorScheme(
    primary = Color(0xFF107C10),
    onPrimary = Color(0xFFFFFFFF),
    primaryContainer = Color(0xFFA6F398),
    onPrimaryContainer = Color(0xFF002201),
    inversePrimary = Color(0xFF6FD75F),
    secondary = Color(0xFF52634F),
    onSecondary = Color(0xFFFFFFFF),
    secondaryContainer = Color(0xFFD5E8CF),
    onSecondaryContainer = Color(0xFF101F0F),
    tertiary = Color(0xFF38656A),
    onTertiary = Color(0xFFFFFFFF),
    tertiaryContainer = Color(0xFFBCEBF0),
    onTertiaryContainer = Color(0xFF002023),
)

/** Material 3 theme for the Compose frontend, themed to Xbox green. */
@Composable
fun xendroidTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit,
) {
    MaterialTheme(
        colorScheme = if (darkTheme) DarkColors else LightColors,
        content = content,
    )
}
