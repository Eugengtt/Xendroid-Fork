package xendroid.compose.ui.content

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import xendroid.compose.ui.content.ContentManagerViewModel.DeleteState
import xendroid.compose.ui.content.ContentManagerViewModel.ListState
import xendroid.compose.ui.library.FolderBrowserScreen

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ContentManagerScreen(
    vm: ContentManagerViewModel,
    gameName: String,
    onBack: () -> Unit,
) {
    val listState by vm.listState.collectAsStateWithLifecycle()
    val installState by vm.state.collectAsStateWithLifecycle()
    val deleteState by vm.deleteState.collectAsStateWithLifecycle()
    var picking by remember { mutableStateOf(false) }
    var selectedTab by rememberSaveable { mutableStateOf(0) }

    if (picking) {
        FolderBrowserScreen(
            onFileChosen = { path ->
                picking = false
                vm.install(path)
            },
            onCancel = { picking = false },
        )
        return
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Text(
                        if (gameName.isNotBlank()) "Content · $gameName" else "Content",
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
                actions = {
                    IconButton(onClick = { picking = true }) {
                        Icon(Icons.Default.Add, contentDescription = "Install content")
                    }
                },
            )
        },
    ) { padding ->
        Column(Modifier.fillMaxSize().padding(padding)) {
            val tabs = listOf("DLC", "Updates")
            TabRow(selectedTabIndex = selectedTab) {
                tabs.forEachIndexed { index, label ->
                    Tab(
                        selected = selectedTab == index,
                        onClick = { selectedTab = index },
                        text = { Text(label) },
                    )
                }
            }
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                when (val s = listState) {
                    ListState.Loading -> CircularProgressIndicator()
                    is ListState.Error -> Text(s.message, modifier = Modifier.padding(24.dp))
                    is ListState.Loaded -> {
                        val items = if (selectedTab == 0) s.dlc else s.updates
                        if (items.isEmpty()) {
                            Text(
                                if (selectedTab == 0) "No DLC installed."
                                else "No title updates installed.",
                                style = MaterialTheme.typography.bodyLarge,
                                modifier = Modifier.padding(24.dp),
                            )
                        } else {
                            ContentList(items = items, onDelete = vm::requestDelete)
                        }
                    }
                }
            }
        }
    }

    ContentInstallDialogs(
        state = installState,
        onDismiss = vm::dismiss,
        onConfirmOverwrite = vm::confirmOverwrite,
    )

    (deleteState as? DeleteState.Confirm)?.let { s ->
        AlertDialog(
            onDismissRequest = vm::dismiss,
            title = { Text("Remove content?") },
            text = {
                Text("Remove “${s.item.displayName}”? This deletes its files for this game.")
            },
            confirmButton = {
                TextButton(onClick = { vm.delete(s.item) }) { Text("Remove") }
            },
            dismissButton = {
                TextButton(onClick = vm::dismiss) { Text("Cancel") }
            },
        )
    }
}

@Composable
private fun ContentList(
    items: List<ContentManagerViewModel.ContentEntry>,
    onDelete: (ContentManagerViewModel.ContentEntry) -> Unit,
) {
    LazyColumn(Modifier.fillMaxSize()) {
        items(items, key = { it.pkgDir }) { item ->
            ListItem(
                headlineContent = {
                    Text(item.displayName, maxLines = 1, overflow = TextOverflow.Ellipsis)
                },
                supportingContent = { Text(humanReadableSize(item.size)) },
                trailingContent = {
                    IconButton(onClick = { onDelete(item) }) {
                        Icon(Icons.Default.Delete, contentDescription = "Remove")
                    }
                },
            )
            HorizontalDivider()
        }
    }
}

private fun humanReadableSize(bytes: Long): String {
    if (bytes < 1024) return "$bytes B"
    val units = arrayOf("KB", "MB", "GB")
    var value = bytes.toDouble() / 1024
    var unit = 0
    while (value >= 1024 && unit < units.lastIndex) {
        value /= 1024
        unit++
    }
    return "%.1f %s".format(value, units[unit])
}
