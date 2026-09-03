package xendroid.compose.core

import xendroid.compose.Utils
import java.io.File

/**
 * Kotlin mirror of the native content-tree layout, used for display + the
 * "already installed?" overwrite check ONLY. The authoritative placement is done
 * natively by InstallContentPackageStandalone.
 *
 *   content/<XUID 016X>/<TitleID 08X>/<ContentType 08X>/<pkgDir>/...
 *
 * Non-profile content (DLC, title updates, ...) lives under machine XUID
 * 0000000000000000 (content_manager.cc:108-112); profiles use their account XUID.
 */
object ContentPaths {
    const val DLC_CONTENT_TYPE = 0x00000002
    const val TU_CONTENT_TYPE = 0x000B0000
    const val PROFILE_CONTENT_TYPE = 0x00010000
    const val MACHINE_XUID = "0000000000000000"

    // Launchable game content types (mirror xenia's XContentType). A container of one
    // of these is a full game the library boots from its CONTAINER file, not add-on content.
    const val INSTALLED_GAME_CONTENT_TYPE = 0x00004000  // GoD
    const val XBOX360_TITLE_CONTENT_TYPE = 0x00007000
    const val ARCADE_TITLE_CONTENT_TYPE = 0x000D0000    // XBLA
    const val GAME_DEMO_CONTENT_TYPE = 0x00080000
    const val COMMUNITY_GAME_CONTENT_TYPE = 0x02000000  // XNA

    /** True for content types that are full, launchable games (vs. add-on content like
     *  DLC, title updates, profiles, themes). The library boots these from the container file. */
    fun isLaunchableGameType(contentType: Int): Boolean = contentType in setOf(
        INSTALLED_GAME_CONTENT_TYPE,
        XBOX360_TITLE_CONTENT_TYPE,
        ARCADE_TITLE_CONTENT_TYPE,
        GAME_DEMO_CONTENT_TYPE,
        COMMUNITY_GAME_CONTENT_TYPE,
    )

    /** content_type -> human label for install UI (XContentType, see content_manager). */
    fun contentTypeLabel(type: Int): String = when (type) {
        0x00000002 -> "DLC"
        0x000B0000 -> "Title update"
        0x00010000 -> "Profile"
        0x000D0000 -> "Arcade game"
        0x00000001 -> "Saved game"
        0x00000003 -> "Publisher"
        else -> "Content"
    }

    /** <storage_root>/content -- matches the core's content_root derivation
     *  (xendroid_emu.cpp:324-326). Utils.get_storage_root_path() returns the same
     *  absolute external app-data path passed to the core as --storage_root. */
    fun contentRoot(): File = File(Utils.get_storage_root_path(), "content")

    /** content/0000000000000000/<TITLE_ID>/<CONTENT_TYPE 08X> under the machine XUID. */
    fun contentDir(titleId: String, contentType: Int): File =
        File(contentRoot(), "$MACHINE_XUID/${titleId.uppercase()}/%08X".format(contentType))
}
