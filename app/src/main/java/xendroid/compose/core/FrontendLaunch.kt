package xendroid.compose.core

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Environment
import android.os.ParcelFileDescriptor
import android.provider.DocumentsContract
import android.provider.MediaStore
import android.util.Log
import java.io.File

/**
 * Resolves the game to boot from a launch intent: the `game_uri` extra, the
 * `AutoStartFile` extra (Dolphin convention used by frontends like ES-DE), or
 * the intent data URI. content:// is translated to a real path when possible,
 * with an open-fd /proc/self/fd passthrough as last resort.
 */
object FrontendLaunch {
    private const val TAG = "FrontendLaunch"

    // /proc/self/fd/<n> must stay valid until process exit.
    private var heldFd: ParcelFileDescriptor? = null

    fun resolveGamePath(context: Context, intent: Intent?): String? {
        intent ?: return null
        intent.getStringExtra("game_uri")?.takeIf { it.isNotBlank() }?.let {
            return normalize(context, it)
        }
        intent.getStringExtra("AutoStartFile")?.takeIf { it.isNotBlank() }?.let {
            Log.i(TAG, "Launching via AutoStartFile extra")
            return normalize(context, it)
        }
        intent.data?.let {
            Log.i(TAG, "Launching via intent data (${it.scheme})")
            return resolveUri(context, it)
        }
        return null
    }

    private fun normalize(context: Context, raw: String): String? =
        if (raw.startsWith("content://") || raw.startsWith("file://")) {
            resolveUri(context, Uri.parse(raw))
        } else {
            raw
        }

    private fun resolveUri(context: Context, uri: Uri): String? = when (uri.scheme) {
        null, "file" -> uri.path
        "content" -> contentToPath(context, uri)
        else -> null
    }

    private fun contentToPath(context: Context, uri: Uri): String? {
        documentsProviderPath(context, uri)?.let { return it }
        smuggledPath(uri)?.let { return it }
        mediaStoreDataPath(context, uri)?.let { return it }
        return fdPassthrough(context, uri)
    }

    /** com.android.externalstorage.documents: "primary:ROMs/x.iso" -> real path. */
    private fun documentsProviderPath(context: Context, uri: Uri): String? {
        if (uri.authority != "com.android.externalstorage.documents") return null
        val docId = runCatching {
            if (DocumentsContract.isDocumentUri(context, uri)) {
                DocumentsContract.getDocumentId(uri)
            } else {
                DocumentsContract.getTreeDocumentId(uri)
            }
        }.getOrNull() ?: return null
        val split = docId.split(":", limit = 2)
        if (split.size != 2) return null
        val base = if (split[0].equals("primary", ignoreCase = true)) {
            Environment.getExternalStorageDirectory().absolutePath
        } else {
            "/storage/${split[0]}"   // removable volume UUID
        }
        return "$base/${split[1]}".takeIf { File(it).isFile }
    }

    /** Many FileProviders embed the real path in the URI path segment. */
    private fun smuggledPath(uri: Uri): String? {
        val p = uri.path ?: return null
        val candidates = buildList {
            add(p)
            add(p.removePrefix("/root"))
            val idx = p.indexOf("/storage/")
            if (idx > 0) add(p.substring(idx))
            val primary = Environment.getExternalStorageDirectory().absolutePath
            val extIdx = p.indexOf("/external_files/")
            if (extIdx >= 0) add("$primary/${p.substring(extIdx + "/external_files/".length)}")
        }
        return candidates.firstOrNull { File(it).isFile }
    }

    private fun mediaStoreDataPath(context: Context, uri: Uri): String? = runCatching {
        context.contentResolver.query(
            uri, arrayOf(MediaStore.MediaColumns.DATA), null, null, null,
        )?.use { cursor ->
            if (cursor.moveToFirst()) cursor.getString(0) else null
        }
    }.getOrNull()?.takeIf { File(it).isFile }

    /** Last resort: hold the provider fd open, boot from /proc/self/fd/<n>. */
    private fun fdPassthrough(context: Context, uri: Uri): String? = runCatching {
        context.contentResolver.openFileDescriptor(uri, "r")?.let { pfd ->
            heldFd = pfd
            val path = "/proc/self/fd/${pfd.fd}"
            Log.i(TAG, "Serving content URI via $path")
            path
        }
    }.getOrNull()
}
