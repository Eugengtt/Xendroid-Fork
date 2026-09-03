package xendroid.compose.ui.content

import android.content.Context
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import xendroid.compose.Emulator
import xendroid.compose.core.ContentPaths
import xendroid.compose.core.EmulatorRuntime
import xendroid.compose.core.GameMetadataSource
import java.io.File

class ContentManagerViewModel(
    private val appContext: Context,
    private val metadata: GameMetadataSource,
    private val titleId: String,
) : ViewModel() {

    data class ContentEntry(
        val pkgDir: String,
        val displayName: String,
        val size: Long,
        val contentType: Int,
    )

    sealed interface ListState {
        data object Loading : ListState
        data class Loaded(val dlc: List<ContentEntry>, val updates: List<ContentEntry>) : ListState
        data class Error(val message: String) : ListState
    }

    private val _listState = MutableStateFlow<ListState>(ListState.Loading)
    val listState: StateFlow<ListState> = _listState.asStateFlow()

    /** Install/overwrite/progress/result reuse the shared ContentInstallState. */
    private val _state = MutableStateFlow<ContentInstallState>(ContentInstallState.Idle)
    val state: StateFlow<ContentInstallState> = _state.asStateFlow()

    /** Delete confirmation stays per-game (separate from the shared install flow). */
    sealed interface DeleteState {
        data object Idle : DeleteState
        data class Confirm(val item: ContentEntry) : DeleteState
    }

    private val _deleteState = MutableStateFlow<DeleteState>(DeleteState.Idle)
    val deleteState: StateFlow<DeleteState> = _deleteState.asStateFlow()

    init { refresh() }

    fun dismiss() {
        _state.value = ContentInstallState.Idle
        _deleteState.value = DeleteState.Idle
    }

    fun refresh() = viewModelScope.launch {
        _listState.value = ListState.Loading
        _listState.value = withContext(Dispatchers.IO) {
            EmulatorRuntime.ensureLoaded()
            val emu = EmulatorRuntime.emulator
                ?: return@withContext ListState.Error("Emulator not loaded.")
            val root = ContentPaths.contentRoot().absolutePath
            try {
                val dlc = emu.list_content(root, titleId, ContentPaths.DLC_CONTENT_TYPE)
                    ?: return@withContext ListState.Error("Couldn't read installed content.")
                val updates = emu.list_content(root, titleId, ContentPaths.TU_CONTENT_TYPE)
                    ?: return@withContext ListState.Error("Couldn't read installed content.")
                ListState.Loaded(
                    dlc = dlc.toEntries(ContentPaths.DLC_CONTENT_TYPE),
                    updates = updates.toEntries(ContentPaths.TU_CONTENT_TYPE),
                )
            } catch (t: RuntimeException) {
                ListState.Error(t.message ?: "Couldn't read installed content.")
            }
        }
    }

    private fun Array<Emulator.ContentItem>.toEntries(contentType: Int) =
        map { ContentEntry(it.pkgDir, it.displayName ?: it.pkgDir, it.size, contentType) }
            .sortedBy { it.displayName.lowercase() }

    /** [srcPath] = absolute host path to the picked package. */
    fun install(srcPath: String) = viewModelScope.launch {
        _state.value = ContentInstallState.Busy("Preparing…")
        val pre = withContext(Dispatchers.IO) { validate(srcPath) }
        when (pre) {
            is PreCheck.Reject -> _state.value = ContentInstallState.Failed(pre.message)
            is PreCheck.Overwrite ->
                _state.value = ContentInstallState.ConfirmOverwrite(srcPath, pre.name)
            is PreCheck.Ok -> runInstall(srcPath, pre.name)
        }
    }

    /** User confirmed overwrite of an existing package dir (OD3). */
    fun confirmOverwrite(srcPath: String, name: String) =
        viewModelScope.launch { runInstall(srcPath, name) }

    fun requestDelete(item: ContentEntry) { _deleteState.value = DeleteState.Confirm(item) }

    fun delete(item: ContentEntry) = viewModelScope.launch {
        _deleteState.value = DeleteState.Idle
        _state.value = ContentInstallState.Busy("Removing…")
        val status = withContext(Dispatchers.IO) {
            val emu = EmulatorRuntime.emulator ?: return@withContext -1
            emu.delete_content(
                ContentPaths.contentRoot().absolutePath, titleId,
                item.contentType, item.pkgDir)
        }
        if (status == 0) {
            _state.value = ContentInstallState.Done("Removed “${item.displayName}”.")
            refresh()
        } else {
            _state.value = ContentInstallState.Failed(deleteReasonFor(status))
        }
    }

    private sealed interface PreCheck {
        data class Ok(val name: String) : PreCheck
        data class Overwrite(val name: String) : PreCheck
        data class Reject(val message: String) : PreCheck
    }

    private suspend fun validate(srcPath: String): PreCheck {
        EmulatorRuntime.ensureLoaded()
        if (!File(srcPath).isFile) return PreCheck.Reject("Couldn't open the package file.")
        val meta = metadata.readContentHeader(srcPath)
            ?: return PreCheck.Reject("Not a recognized content package (need CON/LIVE/PIRS).")
        if (meta.contentType != ContentPaths.DLC_CONTENT_TYPE &&
            meta.contentType != ContentPaths.TU_CONTENT_TYPE)
            return PreCheck.Reject(
                "This package has content type 0x${meta.contentType.toUInt().toString(16)}. " +
                    "Only DLC or title updates can be installed.")
        if (meta.titleId == null || !meta.titleId.equals(titleId, ignoreCase = true))
            return PreCheck.Reject(
                "This package is for title ${meta.titleId ?: "unknown"}, " +
                    "not this game ($titleId).")
        val name = meta.displayName.ifBlank { File(srcPath).name }
        storageShortfall(meta.contentSize)?.let { return PreCheck.Reject(it) }
        val pkgDir = File(ContentPaths.contentDir(titleId, meta.contentType), File(srcPath).name)
        return if (pkgDir.exists()) PreCheck.Overwrite(name) else PreCheck.Ok(name)
    }

    private suspend fun runInstall(srcPath: String, name: String) {
        _state.value = ContentInstallState.Busy("Installing…", 0f)
        // Poll native install progress while the VFS walk blocks an IO thread (the
        // getter reads file-static atomics, so concurrent reads are safe).
        val poll = viewModelScope.launch {
            while (isActive) {
                val p = EmulatorRuntime.emulator?.installProgress() ?: 0f
                (_state.value as? ContentInstallState.Busy)
                    ?.takeIf { it.message == "Installing…" }
                    ?.let { _state.value = it.copy(progress = p.coerceIn(0f, 1f)) }
                delay(200)
            }
        }
        val status = withContext(Dispatchers.IO) {
            val emu = EmulatorRuntime.emulator ?: return@withContext -1
            emu.install_content(srcPath, ContentPaths.contentRoot().absolutePath)
        }
        poll.cancel()
        if (status == 0) {
            _state.value = ContentInstallState.Done(
                "Installed “$name”. It'll be available next time the game boots.")
            refresh()
        } else {
            _state.value = ContentInstallState.Failed(installReasonFor(status))
        }
    }

    private fun deleteReasonFor(status: Int): String = when (status) {
        -1 -> "Emulator not loaded."
        0xC000000D.toInt() -> "Invalid package name."                    // X_STATUS_INVALID_PARAMETER
        0xC0000034.toInt() -> "Already removed."                         // X_STATUS_OBJECT_NAME_NOT_FOUND
        0xC0000022.toInt() -> "Couldn't delete the files."               // X_STATUS_ACCESS_DENIED
        else -> "Delete failed (0x${status.toUInt().toString(16)})."
    }
}
