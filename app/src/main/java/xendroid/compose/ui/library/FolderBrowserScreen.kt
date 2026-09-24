package xendroid.compose.ui.library

import android.content.Context
import android.os.Build
import android.os.Environment
import android.os.storage.StorageManager
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import java.io.File

/** A browsable storage root: internal storage or a mounted removable volume. */
private data class StorageRoot(val label: String, val dir: File)

/**
 * Self-contained java.io.File directory browser for REAL-PATH (All Files Access) mode.
 * Replaces the SAF OPEN_DOCUMENT_TREE picker, which MIUI/HyperOS refuses. Starts at the
 * primary external storage root, lists subdirectories only (sorted, hidden skipped), and
 * lets the user confirm the current directory as the games folder.
 *
 * Mounted removable volumes (USB/SD) get a "volumes" level above the storage roots;
 * enumeration needs API 30+, below that only primary storage is offered.
 *
 * Two modes (the caller picks via which callback it passes):
 *  - folder-pick (default): [onFolderChosen] non-null -> a "Use this folder" button
 *    confirms the current directory; only sub-dirs are listed.
 *  - file-pick: [onFileChosen] non-null -> regular files are ALSO listed and tapping one
 *    returns its absolute path (used by "Install content / DLC" to pick a package).
 *
 * listFiles() may return null for an unreadable directory (e.g. Android/data) even with
 * All Files Access -- that is handled as "no entries" rather than a crash.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun FolderBrowserScreen(
    onFolderChosen: ((path: String) -> Unit)? = null,
    onFileChosen: ((path: String) -> Unit)? = null,
    onCancel: () -> Unit,
) {
    val context = LocalContext.current
    val roots = remember { enumerateStorageRoots(context) }

    // null = the volume-list level.
    var current by remember {
        mutableStateOf<File?>(if (roots.size > 1) null else roots.first().dir)
    }

    // Subdirectories of the current dir: directories only, no hidden, sorted by name.
    val subDirs = remember(current?.absolutePath) {
        current?.listFiles()
            ?.filter { it.isDirectory && !it.isHidden }
            ?.sortedBy { it.name.lowercase() }
            ?: emptyList()
    }

    // File-pick mode only: regular files in the current dir, sorted, hidden skipped.
    val files = remember(current?.absolutePath) {
        val dir = current
        if (onFileChosen == null || dir == null) emptyList()
        else dir.listFiles()
            ?.filter { it.isFile && !it.isHidden }
            ?.sortedBy { it.name.lowercase() }
            ?: emptyList()
    }

    val atVolumeList = current == null
    val atVolumeRoot = current != null &&
        roots.any { it.dir.absolutePath == current!!.absolutePath }

    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Text(
                        current?.absolutePath ?: "Storage",
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        style = MaterialTheme.typography.titleMedium,
                    )
                },
                navigationIcon = {
                    IconButton(
                        onClick = {
                            val dir = current
                            when {
                                dir == null -> onCancel()
                                atVolumeRoot ->
                                    if (roots.size > 1) current = null else onCancel()
                                dir.parentFile != null -> current = dir.parentFile
                                else -> onCancel()
                            }
                        },
                    ) {
                        Icon(
                            Icons.AutoMirrored.Filled.ArrowBack,
                            contentDescription =
                                if (atVolumeList || (atVolumeRoot && roots.size == 1))
                                    "Cancel" else "Up",
                        )
                    }
                },
            )
        },
    ) { padding ->
        Column(Modifier.fillMaxSize().padding(padding)) {
            // Action row: confirm the CURRENT directory (folder-pick only), or cancel.
            Row(
                Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp),
                horizontalArrangement = Arrangement.End,
            ) {
                TextButton(onClick = onCancel) { Text("Cancel") }
                if (onFolderChosen != null && !atVolumeList) {
                    Spacer(Modifier.width(8.dp))
                    TextButton(onClick = { onFolderChosen(current!!.absolutePath) }) {
                        Text("Use this folder")
                    }
                }
            }

            if (atVolumeList) {
                LazyColumn(Modifier.fillMaxSize()) {
                    items(roots, key = { it.dir.absolutePath }) { root ->
                        ListItem(
                            headlineContent = {
                                Text(root.label, maxLines = 1, overflow = TextOverflow.Ellipsis)
                            },
                            supportingContent = {
                                Text(
                                    root.dir.absolutePath,
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis,
                                    style = MaterialTheme.typography.bodySmall,
                                )
                            },
                            trailingContent = {
                                Icon(
                                    Icons.AutoMirrored.Filled.KeyboardArrowRight,
                                    contentDescription = "Open",
                                )
                            },
                            modifier = Modifier.clickable { current = root.dir },
                        )
                    }
                }
            } else if (subDirs.isEmpty() && files.isEmpty()) {
                Column(Modifier.fillMaxSize().padding(24.dp)) {
                    Text(
                        if (onFolderChosen != null)
                            "No sub-folders here. Use \"Use this folder\" to pick this directory."
                        else
                            "Nothing here. Go up and pick another folder.",
                        style = MaterialTheme.typography.bodyMedium,
                    )
                }
            } else {
                LazyColumn(Modifier.fillMaxSize()) {
                    items(subDirs, key = { it.absolutePath }) { dir ->
                        ListItem(
                            headlineContent = {
                                Text(dir.name, maxLines = 1, overflow = TextOverflow.Ellipsis)
                            },
                            trailingContent = {
                                Icon(
                                    Icons.AutoMirrored.Filled.KeyboardArrowRight,
                                    contentDescription = "Open",
                                )
                            },
                            modifier = Modifier.clickable { current = dir },
                        )
                    }
                    if (onFileChosen != null) {
                        items(files, key = { it.absolutePath }) { file ->
                            ListItem(
                                headlineContent = {
                                    Text(file.name, maxLines = 1, overflow = TextOverflow.Ellipsis)
                                },
                                modifier = Modifier.clickable { onFileChosen(file.absolutePath) },
                            )
                        }
                    }
                }
            }
        }
    }
}

/** Primary storage plus mounted removable volumes (API 30+; primary-only below). */
private fun enumerateStorageRoots(context: Context): List<StorageRoot> {
    val primary = Environment.getExternalStorageDirectory() ?: File("/")
    val roots = mutableListOf(StorageRoot("Internal storage", primary))
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
        try {
            val sm = context.getSystemService(Context.STORAGE_SERVICE) as StorageManager
            for (volume in sm.storageVolumes) {
                if (volume.isPrimary) continue
                if (volume.state != Environment.MEDIA_MOUNTED &&
                    volume.state != Environment.MEDIA_MOUNTED_READ_ONLY
                ) {
                    continue
                }
                val dir = volume.directory ?: continue
                roots.add(StorageRoot(volume.getDescription(context) ?: dir.name, dir))
            }
        } catch (_: Exception) {
        }
    }
    return roots
}
