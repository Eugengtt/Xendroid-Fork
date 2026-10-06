package xendroid.compose.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

// Red brand palette (replaces Material's default purple for primary/secondary/tertiary).
// Light uses deep red for white-on-red buttons; dark uses a brighter red
// that reads on dark surfaces. Surfaces/background/error stay Material defaults.
private val DarkColors = darkColorScheme(
    primary = Color(0xFFFF8A80),
    onPrimary = Color(0xFF4B0000),
    primaryContainer = Color(0xFF7A0B0B),
    onPrimaryContainer = Color(0xFFFFDAD4),
    inversePrimary = Color(0xFFC62828),
    secondary = Color(0xFFE7BDB8),
    onSecondary = Color(0xFF3A2523),
    secondaryContainer = Color(0xFF5D3F3B),
    onSecondaryContainer = Color(0xFFF5DDD8),
    tertiary = Color(0xFFA0CFD4),
    onTertiary = Color(0xFF00363B),
    tertiaryContainer = Color(0xFF1E4D52),
    onTertiaryContainer = Color(0xFFBCEBF0),
)
private val LightColors = lightColorScheme(
    primary = Color(0xFFC62828),
    onPrimary = Color(0xFFFFFFFF),
    primaryContainer = Color(0xFFFFDAD4),
    onPrimaryContainer = Color(0xFF410002),
    inversePrimary = Color(0xFFFF8A80),
    secondary = Color(0xFF8F4C46),
    onSecondary = Color(0xFFFFFFFF),
    secondaryContainer = Color(0xFFFFDAD4),
    onSecondaryContainer = Color(0xFF2A1210),
    tertiary = Color(0xFF38656A),
    onTertiary = Color(0xFFFFFFFF),
    tertiaryContainer = Color(0xFFBCEBF0),
    onTertiaryContainer = Color(0xFF002023),
)

/** Material 3 theme for the Compose frontend, themed to red. */
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
