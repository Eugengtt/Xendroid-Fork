package xendroid.compose.ui.userdata

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.provider.DocumentsContract
import android.widget.Toast

/** Opens the app-data dir in the system file UI. Primary route: deep-link to the app's
 *  own DocumentsProvider root (authority = "<applicationId>.DocumentsProvider", which
 *  wraps Application.get_app_data_dir()). Fallback: launch DocumentsUI FilesActivity.
 *  The legacy buildRootUri("$packageName.user", ...) path was BROKEN (literal authority,
 *  no such provider) and is intentionally not ported. */
fun openUserData(context: Context) {
    val authority = "${context.packageName}.DocumentsProvider"
    val rootUri = DocumentsContract.buildRootUri(authority, "root")

    // Try ACTION_VIEW on the provider root (DocumentsUI honors this for browsable roots).
    val view = Intent(Intent.ACTION_VIEW).apply {
        setDataAndType(rootUri, DocumentsContract.Root.MIME_TYPE_ITEM)
        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
    }
    if (tryStart(context, view)) return

    // Fallback: DocumentsUI FilesActivity (the legacy working route).
    for (pkg in listOf("com.android.documentsui", "com.google.android.documentsui")) {
        val files = Intent(Intent.ACTION_VIEW).apply {
            setClassName(pkg, "com.android.documentsui.files.FilesActivity")
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        }
        if (tryStart(context, files)) return
    }
    Toast.makeText(context, "No file manager available", Toast.LENGTH_LONG).show()
}

private fun tryStart(context: Context, intent: Intent): Boolean =
    try { context.startActivity(intent); true }
    catch (e: ActivityNotFoundException) { false }
