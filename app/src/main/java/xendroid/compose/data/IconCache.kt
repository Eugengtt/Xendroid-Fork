package xendroid.compose.data

import java.io.File
import java.security.MessageDigest

/**
 * Kotlin-owned out-of-band icon cache: GOD thumbnail PNG blobs written to
 * cacheDir/game_icons/, read back by Coil AsyncImage over the File. This is NOT
 * the dead native uri_info_list.json. Filename = sha-256(launchUri).png so it's
 * stable and filesystem-safe.
 */
class IconCache(cacheDir: File) {
    private val dir = File(cacheDir, "game_icons").apply { mkdirs() }

    fun fileFor(name: String): File = File(dir, name)

    /** Write the PNG bytes; returns the cache filename (not full path). */
    fun write(launchUri: String, png: ByteArray): String {
        val name = cacheName(launchUri)
        File(dir, name).writeBytes(png)
        return name
    }

    companion object {
        fun cacheName(launchUri: String): String {
            val digest = MessageDigest.getInstance("SHA-256").digest(launchUri.toByteArray())
            return digest.joinToString("") { "%02x".format(it) } + ".png"
        }
    }
}
