package xendroid.compose.data

import android.util.Log
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import java.io.File

/**
 * Persistent per-game metadata cache so a repeat library scan of an UNCHANGED game
 * reuses the extracted display name + icon-cache filename and SKIPS the expensive
 * extraction (ISO/XEX = full native XEX decompress + XDBF/SPA parse; GOD = SAF header
 * read). The icon BYTES are already file-cached by [IconCache]; this caches the
 * EXTRACTION RESULT (name + iconCacheName) keyed by launchUri.
 *
 * Stored as a single JSON file IN cacheDir, alongside [IconCache]'s game_icons/ dir,
 * so an OS cache-clear wipes the metadata cache AND the icon files together (they
 * stay consistent). Loaded once at scan start, mutated during, persisted once after.
 *
 * This is a PURE performance optimization: on any cache problem the caller falls back
 * to extraction, so a cache-cold (or corrupt-cache) run produces identical games.
 */
class GameMetadataCache(cacheDir: File) {
    private val file = File(cacheDir, FILE_NAME)
    private val json = Json { ignoreUnknownKeys = true; encodeDefaults = true }
    private val lock = Any()

    /** One cached extraction result + the file signature it was extracted from. */
    @Serializable
    data class Entry(
        val name: String,
        val iconCacheName: String?,
        val sizeBytes: Long,
        val lastModified: Long,
        val titleId: String? = null,
        val mediaId: String? = null,
        // Extensionless files share one branch (GOD vs STFS); a null format is a legacy
        // entry that predates STFS and is GOD by construction.
        val format: GameFormat? = null,
        val discNumber: Int = 0,
        val discCount: Int = 0,
    )

    @Serializable
    private data class Snapshot(val entries: Map<String, Entry> = emptyMap())

    /** In-memory working copy, populated by [load]. Guarded by [lock]. */
    private val entries = HashMap<String, Entry>()

    /** Load the persisted cache into memory (call once at scan start). Replaces the
     *  current working set. A missing/corrupt file loads as empty (cold cache). */
    fun load() = synchronized(lock) {
        entries.clear()
        if (!file.exists()) return@synchronized
        val loaded = runCatching {
            json.decodeFromString(Snapshot.serializer(), file.readText())
        }.onFailure { warn("metadata cache load failed; treating as cold", it) }
            .getOrNull() ?: return@synchronized
        entries.putAll(loaded.entries)
    }

    /** The cached entry for [launchUri], or null if absent. */
    fun get(launchUri: String): Entry? = synchronized(lock) { entries[launchUri] }

    /** Record an extraction result. A non-cacheable signature (see [Signature.cacheable])
     *  is NOT stored: an unreliable SAF signature must re-extract every scan. */
    fun put(
        launchUri: String,
        name: String,
        iconCacheName: String?,
        signature: Signature,
        titleId: String? = null,
        mediaId: String? = null,
        format: GameFormat? = null,
        discNumber: Int = 0,
        discCount: Int = 0,
    ) {
        if (!signature.cacheable) return
        synchronized(lock) {
            entries[launchUri] =
                Entry(name, iconCacheName, signature.sizeBytes, signature.lastModified, titleId, mediaId,
                      format, discNumber, discCount)
        }
    }

    /** Drop entries whose key is NOT in [liveKeys] (games no longer in the library), so
     *  the cache stays bounded to the current library instead of growing monotonically as
     *  games come and go. Call once per scan (with the current launch uris) before [save]. */
    fun retainOnly(liveKeys: Set<String>) = synchronized(lock) {
        entries.keys.retainAll(liveKeys)
    }

    /** Persist the in-memory cache to disk (call once after the scan). Writes a fresh
     *  snapshot via a temp file + atomic rename so a crash mid-write can't corrupt it. */
    fun save() = synchronized(lock) {
        val snapshot = Snapshot(HashMap(entries))
        runCatching {
            val tmp = File(file.parentFile, "$FILE_NAME.tmp")
            tmp.writeText(json.encodeToString(Snapshot.serializer(), snapshot))
            if (!tmp.renameTo(file)) {
                // Some filesystems reject rename-over; fall back to a direct write.
                file.writeText(tmp.readText())
                tmp.delete()
            }
        }.onFailure { warn("metadata cache save failed", it) }
    }

    /** Log a warning, swallowing the android.util.Log "not mocked" RuntimeException that
     *  the JVM unit-test stub throws -- the cache must survive a log failure (it's only a
     *  perf cache) so a parse/IO problem always degrades to a cold cache, never a crash. */
    private fun warn(msg: String, t: Throwable) {
        runCatching { Log.w(TAG, msg, t) }
    }

    /**
     * A game file's change signature. Both fields come from the real host file
     * (File.length() / lastModified()), which MAY return 0 or -1 for either field. Such a signature is
     * [cacheable] == false: we can't detect a change, so we never cache it (extract every
     * scan). Only a signature with BOTH fields strictly > 0 is trustworthy.
     */
    @Serializable
    data class Signature(val sizeBytes: Long, val lastModified: Long) {
        val cacheable: Boolean get() = sizeBytes > 0 && lastModified > 0
    }

    companion object {
        private const val TAG = "GameMetadataCache"
        // v5: v4 caches hold add-on content the deep scan published as games before the
        // content-type gate; a hit would resurrect it without ever reclassifying.
        const val FILE_NAME = "game_metadata_v5.json"

        /**
         * PURE, side-effect-free HIT/MISS decision (no SAF/JNI/IO) so it is unit-testable.
         * Returns a [Hit] (reuse the cached name + iconCacheName, SKIP extraction) ONLY when
         * ALL hold:
         *  (a) [cached] != null (an entry exists for this launchUri),
         *  (b) [signature].cacheable AND it matches the cached signature (same sizeBytes &&
         *      lastModified) -- a non-cacheable (0/-1) signature is always a MISS,
         *  (c) if the cached iconCacheName != null, [iconFileExists] is true (the icon File
         *      survived; covers a partially-cleared cache).
         * Otherwise [Miss] -> extract as normal.
         */
        fun decide(
            cached: Entry?,
            signature: Signature,
            iconFileExists: (iconCacheName: String) -> Boolean,
        ): Decision {
            if (cached == null) return Decision.Miss
            if (!signature.cacheable) return Decision.Miss
            if (cached.sizeBytes != signature.sizeBytes ||
                cached.lastModified != signature.lastModified
            ) {
                return Decision.Miss
            }
            val icon = cached.iconCacheName
            if (icon != null && !iconFileExists(icon)) return Decision.Miss
            return Decision.Hit(cached.name, cached.iconCacheName, cached.titleId, cached.mediaId,
                                cached.format, cached.discNumber, cached.discCount)
        }
    }

    /** Outcome of [decide]. [Hit] carries the cached values to build the Game from. */
    sealed interface Decision {
        data object Miss : Decision
        data class Hit(
            val name: String,
            val iconCacheName: String?,
            val titleId: String? = null,
            val mediaId: String? = null,
            val format: GameFormat? = null,
            val discNumber: Int = 0,
            val discCount: Int = 0,
        ) : Decision
    }
}
