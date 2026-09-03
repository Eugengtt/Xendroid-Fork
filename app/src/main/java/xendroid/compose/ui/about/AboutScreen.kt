package xendroid.compose.ui.about

import android.webkit.WebView
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import xendroid.compose.Emulator

private const val TITLE = "Xendroid — Xbox 360 emulation on Android."

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AboutScreen(onBack: () -> Unit) {
    val context = LocalContext.current
    var showLicenses by remember { mutableStateOf(false) }

    val versionName = remember {
        runCatching {
            context.packageManager.getPackageInfo(context.packageName, 0).versionName
        }.getOrNull() ?: "?"
    }
    // simple_device_info() is a JNI instance method; Emulator.get may be null before
    // load_library() on delay-load devices. Guard it.
    val deviceInfo = remember {
        runCatching { Emulator.get?.simple_device_info() }.getOrNull()
            ?: "Device info unavailable (emulator not loaded yet)."
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("About") },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
            )
        }
    ) { padding ->
        Column(
            Modifier.fillMaxSize().padding(padding).padding(16.dp)
                .verticalScroll(rememberScrollState()),
        ) {
            Text("xendroid", style = MaterialTheme.typography.headlineSmall)
            Text("Version $versionName", style = MaterialTheme.typography.bodyMedium)
            Spacer(Modifier.height(16.dp))

            Text("Credits", style = MaterialTheme.typography.titleMedium)
            Text(TITLE, style = MaterialTheme.typography.bodyMedium)
            Spacer(Modifier.height(16.dp))

            Text("Device", style = MaterialTheme.typography.titleMedium)
            SelectionContainerText(deviceInfo)
            Spacer(Modifier.height(16.dp))

            Button(onClick = { showLicenses = true }) { Text("Open-source licenses") }
        }
    }

    if (showLicenses) {
        AlertDialog(
            onDismissRequest = { showLicenses = false },
            confirmButton = { TextButton(onClick = { showLicenses = false }) { Text("OK") } },
            title = { Text("Licenses") },
            text = {
                AndroidView(
                    factory = { ctx ->
                        WebView(ctx).apply { loadUrl("file:///android_asset/licenses.html") }
                    },
                    modifier = Modifier.fillMaxWidth().height(400.dp),
                )
            },
        )
    }
}

@Composable
private fun SelectionContainerText(text: String) {
    SelectionContainer {
        Text(text, style = MaterialTheme.typography.bodySmall)
    }
}
