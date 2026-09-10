package xendroid.compose.ui.content

import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import xendroid.compose.ui.library.FolderBrowserScreen

/**
 * Global install-only flow: pick a package, install it, pop back to the library.
 * The action is pick -> install, so the folder browser is presented immediately and
 * we pop back once the install completes (Done) or is cancelled. Reuses the shared
 * ContentInstallState + ContentInstallDialogs (no per-game list/delete here).
 */
@Composable
fun InstallContentScreen(
    vm: InstallContentViewModel,
    onBack: () -> Unit,
    sourcePath: String? = null,
) {
    val installState by vm.state.collectAsStateWithLifecycle()

    // Launched for a known source (a disc the library recognised as carrying content):
    // install it directly instead of asking the user to find the file again.
    LaunchedEffect(sourcePath) { if (sourcePath != null) vm.install(sourcePath) }

    // Only show the picker while no install is in flight; the dialogs take over after.
    if (sourcePath == null && installState is ContentInstallState.Idle) {
        FolderBrowserScreen(
            onFileChosen = { path -> vm.install(path) },
            onCancel = onBack,
        )
    }

    ContentInstallDialogs(
        state = installState,
        onDismiss = {
            // Done dismissal returns to the library; Failed/Overwrite just clear back to Idle.
            val finished = installState is ContentInstallState.Done
            vm.dismiss()
            if (finished) onBack()
        },
        onConfirmOverwrite = vm::confirmOverwrite,
    )
}
