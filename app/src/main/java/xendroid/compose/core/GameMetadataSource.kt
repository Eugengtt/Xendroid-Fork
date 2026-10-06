package xendroid.compose.core

import android.util.Log
import xendroid.compose.Emulator
import xendroid.compose.data.GameFormat

/**
 * Wraps the synchronous mmap-based native metadata reads off an absolute host path
 * (All Files Access / real-path mode). MUST run off the main thread (Dispatchers.IO).
 * Returns null for any non-matching/unreadable container (native returns null, not throws,
 * but it is declared `throws RuntimeException` so we defend anyway).
 */
class GameMetadataSource {

    /** Parsed GOD header: title + raw embedded PNG bytes (may be empty []) + the
     *  8-char uppercase-hex title id (null for an unreadable container). */
    data class GodMeta(val name: String, val iconPng: ByteArray?, val titleId: String?, val mediaId: String?,
                       val discNumber: Int = 0, val discCount: Int = 0)

    /** Parsed boot-free XEX meta: title (may be "" -> caller uses filename fallback),
     *  raw embedded PNG bytes (null/empty -> app_icon fallback), and the 8-char
     *  uppercase-hex title id (null when unreadable / 00000000). */
    data class XexMeta(val name: String, val iconPng: ByteArray?, val titleId: String?, val mediaId: String?,
                       val discNumber: Int = 0, val discCount: Int = 0)

    // ---- Real-path (All Files Access) reads: call the path natives (no Context: real-path
    // devices mount directly from a path). path = the absolute host path (ISO file /
    // default.xex / .zar / GOD container).

    /** Boot-free title-id read from a real path for ISO / XEX_FOLDER / ZAR. */
    fun readTitleIdPath(path: String, format: GameFormat): String? {
        val code = format.titleIdCode ?: return null
        val emu = EmulatorRuntime.emulator ?: return null
        return try {
            emu.title_id_from_path(path, code)
                ?.takeIf { it.isNotBlank() && it != "00000000" }
        } catch (t: RuntimeException) {
            Log.w("GameMetadataSource", "title_id read failed for $path ($format)", t)
            null
        } catch (t: LinkageError) {
            warnMissingNative("title_id_from_path", t); null
        }
    }

    /** Boot-free combined name+icon(+titleId) read from a real path for ISO/XEX_FOLDER/ZAR. */
    fun readXexMetaPath(path: String, format: GameFormat): XexMeta? {
        if (format != GameFormat.ISO && format != GameFormat.XEX_FOLDER &&
            format != GameFormat.ZAR) return null
        val code = format.titleIdCode ?: return null
        val emu = EmulatorRuntime.emulator ?: return null
        return try {
            val info: Emulator.GameInfo = emu.meta_from_path(path, code) ?: return null
            XexMeta(
                name = info.name ?: "",
                iconPng = info.icon?.takeIf { it.isNotEmpty() },
                titleId = info.titleId?.takeIf { it.isNotBlank() && it != "00000000" },
                mediaId = info.mediaId?.takeIf { it.isNotBlank() && it != "00000000" },
                discNumber = info.discNumber,
                discCount = info.discCount,
            )
        } catch (t: RuntimeException) {
            Log.w("GameMetadataSource", "XEX meta extraction failed for $path ($format)", t)
            null
        } catch (t: LinkageError) {
            warnMissingNative("meta_from_path", t); null
        }
    }

    /** Parsed STFS/SVOD content-package header: title id (8-char hex, null when 0),
     *  content type (raw, e.g. 0x2 for DLC), payload size, display name, icon. */
    data class ContentMeta(
        val titleId: String?,
        val contentType: Int,
        val contentSize: Long,
        val displayName: String,
        val iconPng: ByteArray?,
    )

    /** Header-only probe of a content package (CON/LIVE/PIRS) from a real path.
     *  Returns null for a non-content/unreadable container. Off-main. */
    fun readContentHeader(path: String): ContentMeta? {
        val emu = EmulatorRuntime.emulator ?: return null
        return try {
            val info: Emulator.ContentInfo = emu.content_header(path) ?: return null
            ContentMeta(
                titleId = info.titleId.takeIf { it != 0 }?.let { "%08X".format(it) },
                contentType = info.contentType,
                contentSize = info.contentSize,
                displayName = info.displayName ?: "",
                iconPng = info.icon?.takeIf { it.isNotEmpty() },
            )
        } catch (t: RuntimeException) {
            Log.w("GameMetadataSource", "content header read failed for $path", t)
            null
        } catch (t: LinkageError) {
            warnMissingNative("content_header", t); null
        }
    }

    /** One installable package inside a disc image (see [listDiscContent]). */
    data class DiscContent(
        val innerPath: String,
        val displayName: String,
        val titleId: String?,
        val contentType: Int,
        val size: Long,
    )

    /** Packages a disc image carries under \content\ -- what a mandatory-install title
     *  (GTA V and friends) expects on the HDD before it will run. Empty for an ordinary
     *  disc. Off-main: walks the image. */
    fun listDiscContent(path: String): List<DiscContent> {
        val emu = EmulatorRuntime.emulator ?: return emptyList()
        return try {
            emu.list_disc_content(path).orEmpty().map { item ->
                DiscContent(
                    innerPath = item.innerPath,
                    displayName = item.displayName ?: "",
                    titleId = item.titleId.takeIf { it != 0 }?.let { "%08X".format(it) },
                    contentType = item.contentType,
                    size = item.size,
                )
            }
        } catch (t: RuntimeException) {
            Log.w("GameMetadataSource", "disc content listing failed for $path", t)
            emptyList()
        } catch (t: LinkageError) {
            warnMissingNative("list_disc_content", t); emptyList()
        }
    }

    /** GOD container header read from a real path (title + icon + title id). */
    fun readGodPath(path: String): GodMeta? {
        val emu = EmulatorRuntime.emulator ?: return null
        return try {
            val info: Emulator.GameInfo = emu.meta_info_from_god_path(path) ?: return null
            val icon = info.icon?.takeIf { it.isNotEmpty() }
            GodMeta(
                name = info.name ?: "",
                iconPng = icon,
                titleId = info.titleId,
                mediaId = info.mediaId?.takeIf { it.isNotBlank() && it != "00000000" },
                discNumber = info.discNumber,
                discCount = info.discCount,
            )
        } catch (t: RuntimeException) {
            Log.w("GameMetadataSource", "GOD parse failed for $path", t)
            null
        } catch (t: LinkageError) {
            warnMissingNative("meta_info_from_god_path", t); null
        }
    }

    /** The path scan natives (section 1A) may not be built into libe.so yet -- a call then
     *  throws UnsatisfiedLinkError (a LinkageError, NOT a RuntimeException). Swallow it so the
     *  real-path scan degrades to filename + app_icon (the shippable v1 tier) instead of
     *  crashing the whole scan. Remove the LinkageError catches once the natives ship. */
    private fun warnMissingNative(method: String, t: LinkageError) {
        runCatching { Log.w("GameMetadataSource", "native $method unavailable (not built yet)", t) }
    }
}
