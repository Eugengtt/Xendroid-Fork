package xendroid.compose.ui.settings

import android.app.Activity
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.selection.selectable
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import xendroid.compose.Utils
import xendroid.compose.core.SessionLogs
import xendroid.compose.driver.DriverInfo
import xendroid.compose.driver.DriverRepository
import xendroid.compose.settings.Setting
import xendroid.compose.settings.SettingsHost

@Composable
fun SettingRow(host: SettingsHost, s: Setting, modified: Boolean, raw: String? = null) = when (s) {
    is Setting.Bool       -> BoolRow(host, s, modified)
    is Setting.IntRange   -> IntRow(host, s, modified)
    is Setting.ListChoice -> ListRow(host, s, modified)
    is Setting.Action     ->
        if (s.name == "dump_session_logs") ExportLogsRow(s)
        else DriverActionRow(host, s, modified, raw)
}

@Composable
private fun titleColor(modified: Boolean) =
    if (modified) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.onSurface

@Composable
private fun RowTitle(text: String, modified: Boolean, sub: String? = null, desc: String = "") {
    Column {
        Text(text, color = titleColor(modified), style = MaterialTheme.typography.bodyLarge)
        if (desc.isNotEmpty()) {
            Text(
                desc,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
        if (sub != null) {
            Text(
                sub,
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.primary
            )
        }
    }
}

@Composable
private fun RowValue(value: String) {
    Text(
        value,
        style = MaterialTheme.typography.bodyMedium,
        color = MaterialTheme.colorScheme.primary,
        modifier = Modifier.padding(start = 12.dp)
    )
}

@Composable
private fun BoolRow(host: SettingsHost, s: Setting.Bool, modified: Boolean) {
    val checked = remember(modified) { host.currentBool(s) }
    var local by remember { mutableStateOf(checked) }

    Row(
        Modifier
            .fillMaxWidth()
            .clickable {
                local = !local
                host.onBoolChanged(s, local)
            }
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.weight(1f)) {
            RowTitle(s.title, modified, desc = s.desc)
        }

        Switch(
            checked = local,
            onCheckedChange = {
                local = it
                host.onBoolChanged(s, it)
            }
        )
    }
}

@Composable
private fun IntRow(host: SettingsHost, s: Setting.IntRange, modified: Boolean) {
    var showDialog by remember { mutableStateOf(false) }
    val current = host.currentInt(s)

    Row(
        Modifier
            .fillMaxWidth()
            .clickable { showDialog = true }
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(Modifier.weight(1f)) {
            RowTitle(s.title, modified, desc = s.desc)
        }

        RowValue(current.toString())
    }

    if (showDialog) {
        var slider by remember {
            mutableFloatStateOf(
                current.coerceIn(s.min, s.max).toFloat()
            )
        }

        AlertDialog(
            onDismissRequest = { showDialog = false },
            title = { Text(s.title) },
            text = {
                Column {
                    Text(
                        slider.toInt().toString(),
                        style = MaterialTheme.typography.titleLarge
                    )

                    Slider(
                        value = slider,
                        onValueChange = { slider = it },
                        valueRange = s.min.toFloat()..s.max.toFloat(),
                        steps = (s.max - s.min - 1).coerceAtLeast(0)
                    )
                }
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        host.onIntChanged(s, slider.toInt())
                        showDialog = false
                    }
                ) {
                    Text("OK")
                }
            },
            dismissButton = {
                TextButton(
                    onClick = { showDialog = false }
                ) {
                    Text("Cancel")
                }
            },
        )
    }
}

@Composable
private fun ListRow(host: SettingsHost, s: Setting.ListChoice, modified: Boolean) {
    var showDialog by remember { mutableStateOf(false) }
    val currentValue = host.currentListValue(s)

    val currentLabel =
        s.options.firstOrNull { it.value == currentValue }?.label
            ?: if (currentValue.isEmpty()) "(default)" else currentValue

    Row(
        Modifier
            .fillMaxWidth()
            .clickable { showDialog = true }
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(Modifier.weight(1f)) {
            RowTitle(s.title, modified, desc = s.desc)
        }

        RowValue(currentLabel)
    }

    if (showDialog) {
        val listState = rememberLazyListState()
        val selectedIndex = s.options.indexOfFirst { it.value == currentValue }

        LaunchedEffect(Unit) {
            if (selectedIndex > 0) {
                listState.scrollToItem(selectedIndex)
            }
        }

        AlertDialog(
            onDismissRequest = { showDialog = false },
            title = { Text(s.title) },
            text = {
                LazyColumn(
                    state = listState,
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(max = 400.dp)
                ) {
                    items(s.options, key = { it.value }) { opt ->
                        Row(
                            Modifier
                                .fillMaxWidth()
                                .selectable(
                                    selected = opt.value == currentValue,
                                    onClick = {
                                        host.onListChanged(s, opt.value)
                                        showDialog = false
                                    }
                                )
                                .padding(vertical = 10.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            RadioButton(
                                selected = opt.value == currentValue,
                                onClick = {
                                    host.onListChanged(s, opt.value)
                                    showDialog = false
                                }
                            )

                            Spacer(Modifier.width(8.dp))
                            Text(opt.label)
                        }
                    }
                }
            },
            confirmButton = {},
            dismissButton = {
                TextButton(onClick = { showDialog = false }) {
                    Text("Cancel")
                }
            },
        )
    }
}

@Composable
private fun ExportLogsRow(s: Setting.Action) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var busy by remember { mutableStateOf(false) }

    Row(
        Modifier
            .fillMaxWidth()
            .clickable(enabled = !busy) {
                busy = true

                scope.launch {
                    val dest = withContext(Dispatchers.IO) {
                        runCatching {
                            SessionLogs.exportAll()
                        }.getOrNull()
                    }

                    Toast.makeText(
                        context,
                        dest?.let {
                            "Logs exported to ${it.name} in Downloads"
                        } ?: "No logs to export",
                        Toast.LENGTH_LONG
                    ).show()

                    busy = false
                }
            }
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.weight(1f)) {
            RowTitle(
                s.title,
                false,
                sub = if (busy) "Exporting..." else "Shelved sessions + current run",
                desc = s.desc
            )
        }
    }
}

@Composable
private fun DriverActionRow(
    host: SettingsHost,
    s: Setting.Action,
    modified: Boolean,
    raw: String?
) {
    if (!host.isCustomDriverSupported) return

    val context = LocalContext.current
    val activity = context as? Activity
    val scope = rememberCoroutineScope()
    val current = raw ?: host.currentDriverPath(s)

    val prefs = remember {
        context.getSharedPreferences(
            "xendroid_driver_manager",
            android.content.Context.MODE_PRIVATE
        )
    }

    var showManager by remember { mutableStateOf(false) }
    var drivers by remember { mutableStateOf<List<DriverInfo>>(emptyList()) }
    var loading by remember { mutableStateOf(false) }
    var downloading by remember { mutableStateOf<String?>(null) }
    var progress by remember { mutableIntStateOf(0) }

    val pickZip = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (uri != null && activity != null) {
            Utils.install_custom_driver_from_zip(activity, uri) { path ->
                host.onDriverPathChanged(s, path)
            }
        }
    }

    fun loadDrivers() {
        if (loading || drivers.isNotEmpty()) return

        loading = true

        scope.launch {
            runCatching {
                DriverRepository.loadDrivers()
            }.onSuccess {
                drivers = it
            }.onFailure {
                Toast.makeText(
                    context,
                    "Failed to load drivers: ${it.message}",
                    Toast.LENGTH_LONG
                ).show()
            }

            loading = false
        }
    }

    Column(Modifier.fillMaxWidth()) {

        Row(
            Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(Modifier.weight(1f)) {
                RowTitle(
                    s.title,
                    modified,
                    sub = current.ifEmpty { "Default" },
                    desc = s.desc
                )
            }
        }

        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.Start
        ) {
            TextButton(
                onClick = {
                    showManager = true
                    loadDrivers()
                }
            ) {
                Text("Driver Manager")
            }

            TextButton(
                onClick = {
                    host.onDriverPathChanged(s, "")
                }
            ) {
                Text("Use default driver")
            }

            TextButton(
                onClick = {
                    pickZip.launch(arrayOf("application/zip"))
                }
            ) {
                Text("Import ZIP")
            }
        }
    }

    if (showManager) {
        AlertDialog(
            onDismissRequest = {
                if (downloading == null) {
                    showManager = false
                }
            },
            title = {
                Text("Custom GPU Drivers")
            },
            text = {
                when {
                    loading -> {
                        Box(
                            Modifier.fillMaxWidth(),
                            contentAlignment = Alignment.Center
                        ) {
                            CircularProgressIndicator()
                        }
                    }

                    drivers.isEmpty() -> {
                        Text("No drivers available.")
                    }

                    else -> {
                        LazyColumn(
                            modifier = Modifier
                                .fillMaxWidth()
                                .heightIn(max = 450.dp)
                        ) {
                            items(
                                drivers,
                                key = { "${it.name}_${it.url}" }
                            ) { driver ->

                                val installedPath =
                                    prefs.getString(
                                        "installed_${driver.url}",
                                        null
                                    )?.takeIf {
                                        java.io.File(it).exists()
                                    }

                                Column(
                                    Modifier
                                        .fillMaxWidth()
                                        .padding(vertical = 8.dp)
                                ) {
                                    Text(
                                        driver.name,
                                        style = MaterialTheme.typography.bodyLarge
                                    )

                                    Text(
                                        driver.version,
                                        style = MaterialTheme.typography.bodySmall,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant
                                    )

                                    if (downloading == driver.url) {
                                        Spacer(Modifier.height(8.dp))

                                        LinearProgressIndicator(
                                            progress = { progress / 100f },
                                            modifier = Modifier.fillMaxWidth()
                                        )

                                        Text("$progress%")
                                    } else {
                                        TextButton(
                                            enabled = downloading == null && activity != null,
                                            onClick = {
                                                if (activity == null) {
                                                    return@TextButton
                                                }

                                                if (installedPath != null) {
                                                    host.onDriverPathChanged(
                                                        s,
                                                        installedPath
                                                    )

                                                    Toast.makeText(
                                                        context,
                                                        "Driver selected",
                                                        Toast.LENGTH_SHORT
                                                    ).show()

                                                    return@TextButton
                                                }

                                                downloading = driver.url
                                                progress = 0

                                                scope.launch {
                                                    runCatching {
                                                        DriverRepository.downloadDriver(
                                                            context,
                                                            driver
                                                        ) {
                                                            progress = it
                                                        }
                                                    }.onSuccess { file ->

                                                        val installed =
                                                            Utils.install_custom_driver_from_file(
                                                                activity,
                                                                file
                                                            ) { path ->

                                                                prefs.edit()
                                                                    .putString(
                                                                        "installed_${driver.url}",
                                                                        path
                                                                    )
                                                                    .apply()

                                                                host.onDriverPathChanged(
                                                                    s,
                                                                    path
                                                                )
                                                            }

                                                        if (installed) {
                                                            Toast.makeText(
                                                                context,
                                                                "Driver installed",
                                                                Toast.LENGTH_SHORT
                                                            ).show()
                                                        }
                                                    }.onFailure {
                                                        Toast.makeText(
                                                            context,
                                                            "Download failed: ${it.message}",
                                                            Toast.LENGTH_LONG
                                                        ).show()
                                                    }

                                                    downloading = null
                                                    progress = 0
                                                }
                                            }
                                        ) {
                                            Text(
                                                if (installedPath != null) {
                                                    "Use"
                                                } else {
                                                    "Download & Install"
                                                }
                                            )
                                        }
                                    }

                                    HorizontalDivider()
                                }
                            }
                        }
                    }
                }
            },
            confirmButton = {
                TextButton(
                    enabled = downloading == null,
                    onClick = {
                        showManager = false
                    }
                ) {
                    Text("Close")
                }
            }
        )
    }
}

@Composable
fun OverrideRow(
    host: SettingsHost,
    s: Setting,
    overrideValue: String?,
    onOverrideToggle: (Boolean) -> Unit,
) {
    val overridden = overrideValue != null

    Row(
        Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Switch(
            checked = overridden,
            onCheckedChange = onOverrideToggle,
            modifier = Modifier.padding(start = 12.dp)
        )

        Box(Modifier.weight(1f)) {
            if (overridden) {
                key(overrideValue) {
                    SettingRow(host, s, modified = true)
                }
            } else {
                InheritedPreview(host, s)
            }
        }
    }
}

@Composable
private fun InheritedPreview(host: SettingsHost, s: Setting) {
    val grey = MaterialTheme.colorScheme.onSurfaceVariant

    when (s) {
        is Setting.Bool -> Row(
            Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(Modifier.weight(1f)) {
                Text(
                    s.title,
                    color = grey,
                    style = MaterialTheme.typography.bodyLarge
                )

                if (s.desc.isNotEmpty()) {
                    Text(
                        s.desc,
                        color = grey,
                        style = MaterialTheme.typography.bodySmall
                    )
                }
            }

            Switch(
                checked = host.currentBool(s),
                onCheckedChange = null,
                enabled = false
            )
        }

        is Setting.IntRange ->
            InheritedTextRow(
                s.title,
                host.currentInt(s).toString(),
                s.desc,
                grey
            )

        is Setting.ListChoice -> {
            val v = host.currentListValue(s)

            val label =
                s.options.firstOrNull { it.value == v }?.label
                    ?: v.ifEmpty { "(default)" }

            InheritedTextRow(
                s.title,
                label,
                s.desc,
                grey
            )
        }

        is Setting.Action ->
            InheritedTextRow(
                s.title,
                host.currentDriverPath(s).ifEmpty { "Default" },
                s.desc,
                grey
            )
    }
}

@Composable
private fun InheritedTextRow(
    title: String,
    value: String,
    desc: String,
    grey: Color
) {
    Column(
        Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 12.dp)
    ) {
        Text(
            title,
            color = grey,
            style = MaterialTheme.typography.bodyLarge
        )

        if (desc.isNotEmpty()) {
            Text(
                desc,
                color = grey,
                style = MaterialTheme.typography.bodySmall
            )
        }

        Text(
            value,
            color = grey,
            style = MaterialTheme.typography.bodySmall
        )
    }
}