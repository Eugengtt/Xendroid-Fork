package xendroid.compose.core

import java.io.File

object ProfilePaths {
    private const val DASHBOARD_STRING_ID = "FFFE07D1"
    private const val PROFILE_CONTENT_TYPE = "00010000"
    val XUID_REGEX = Regex("^[0-9A-F]{16}$")

    fun profileDir(xuid: String): File {
        val id = xuid.uppercase()
        return File(ContentPaths.contentRoot(), "$id/$DASHBOARD_STRING_ID/$PROFILE_CONTENT_TYPE/$id")
    }

    fun tile64Path(xuid: String): File = File(profileDir(xuid), "tile_64.png")
    fun tile32Path(xuid: String): File = File(profileDir(xuid), "tile_32.png")
}
