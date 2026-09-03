package xendroid.compose.core

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.Settings

/**
 * All Files Access (MANAGE_EXTERNAL_STORAGE) helper for the (only) real-path library mode.
 *
 * The user grants All Files Access and the library scans/boots from a real filesystem path
 * (no content:// uris, no /proc/self/fd open that breaks .zar).
 *
 * The permission + [Environment.isExternalStorageManager] are API 30 (R) only; on API 29
 * [isSupported] is false and no games path is available. There is no result payload for the
 * grant -- callers re-check [isGranted] on ON_START after returning from Settings.
 */
object AllFilesAccess {

    /** True on API 30+ (R), where MANAGE_EXTERNAL_STORAGE / the manager check exist. */
    val isSupported: Boolean
        get() = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R

    /** Whether this app currently holds All Files Access. Always false below API 30.
     *  (Inline SDK_INT check, not [isSupported], so lint can trace the API-30 guard.) */
    fun isGranted(): Boolean =
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && Environment.isExternalStorageManager()

    /**
     * Launch the system "All files access" settings screen for this app. Prefers the
     * app-scoped page (lands directly on our toggle); on devices that don't expose it
     * (ActivityNotFoundException) falls back to the global manage-all-files list.
     * No-op on API < 30. Grant is observed by re-checking [isGranted] on ON_START.
     */
    fun requestAccess(context: Context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return
        // Per-app page (lands directly on our toggle). Constructed inline so lint can trace
        // the API-30 guard above through to the API-30 Settings constant.
        val appPage = Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION).apply {
            data = Uri.fromParts("package", context.packageName, null)
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        }
        try {
            context.startActivity(appPage)
        } catch (e: ActivityNotFoundException) {
            // Some devices don't expose the per-app page; fall back to the global list.
            val globalPage = Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)
                .apply { addFlags(Intent.FLAG_ACTIVITY_NEW_TASK) }
            runCatching { context.startActivity(globalPage) }
        }
    }
}
