package xendroid.compose.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import xendroid.compose.data.IconCache

class IconCacheTest {
    @Test fun cacheNameIsStableForSameUri() {
        assertEquals(IconCache.cacheName("content://x/5841"), IconCache.cacheName("content://x/5841"))
    }
    @Test fun cacheNameDiffersForDifferentUris() {
        assertTrue(IconCache.cacheName("content://a") != IconCache.cacheName("content://b"))
    }
    @Test fun cacheNameIsFilesystemSafe() {
        assertTrue(IconCache.cacheName("content://x/ y?z").all { it.isLetterOrDigit() || it == '.' })
        assertTrue(IconCache.cacheName("content://x").endsWith(".png"))
    }
}
