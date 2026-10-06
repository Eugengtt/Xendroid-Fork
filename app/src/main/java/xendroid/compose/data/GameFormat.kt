package xendroid.compose.data

/** Container formats the library scan recognizes. XEX_FOLDER is a directory, not a
 *  filename match, so it is handled separately. */
enum class GameFormat {
    ISO, ZAR, GOD, XEX_FOLDER, STFS;

    /** Display name from a *file* name (XEX folders use the folder name verbatim). */
    fun displayNameFor(fileName: String): String = when (this) {
        ISO, ZAR -> fileName.dropLast(4)
        GOD, XEX_FOLDER, STFS -> fileName  // STFS containers are extensionless
    }

    /** Native title-id format code (MUST match the enum in emulator_xendroid.cpp).
     *  null for formats with no boot-free reader via title_id_from_path. */
    val titleIdCode: Int? get() = when (this) {
        ISO -> 0
        XEX_FOLDER -> 1
        ZAR -> 2
        GOD -> null        // GOD uses its own GameInfo reader
        STFS -> null       // STFS reads its title id from the content_header
    }

    companion object {
        /** File-name -> format, or null if ignored. Detection order matches the scan:
         *  .iso, then .zar, then extensionless => GOD. */
        fun fromFileName(name: String): GameFormat? {
            val lower = name.lowercase()
            return when {
                lower.endsWith(".iso") -> ISO
                lower.endsWith(".zar") -> ZAR
                !name.contains('.') -> GOD
                else -> null
            }
        }
    }
}
