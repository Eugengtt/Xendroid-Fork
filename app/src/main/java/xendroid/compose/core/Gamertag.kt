package xendroid.compose.core

object Gamertag {
    private val PATTERN = Regex("^[A-Za-z][A-Za-z0-9]*( [A-Za-z0-9]+)*$")

    fun isValid(gamertag: String): Boolean =
        gamertag.length in 1..15 && PATTERN.matches(gamertag)
}
