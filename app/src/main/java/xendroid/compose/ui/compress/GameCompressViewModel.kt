package xendroid.compose.ui.compress

import android.content.Context
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import xendroid.compose.core.EmulatorRuntime
import java.io.File

/**
 * Drives the per-game "Compress to .zar" safe-replace flow. Deleting the original .iso is
 * offered only after a verified .zar has been placed beside it, and happens only on explicit
 * confirmation; every failure / early-return path leaves the .iso intact. The native call
 * blocks, so it runs on [Dispatchers.IO]; the temp .zar lives in cacheDir and is always
 * removed in a finally.
 */
class GameCompressViewModel(
    private val appContext: Context,
) : ViewModel() {

    sealed interface CompressState {
        data object Idle : CompressState
        data class Busy(val message: String, val progress: Float = -1f) : CompressState
        /** The verified .zar is in place and the .iso is STILL there; the user decides. */
        data class ConfirmDelete(
            val isoPath: String,
            val isoName: String,
            val zarName: String,
            val isoBytes: Long,
        ) : CompressState
        data class Done(val message: String) : CompressState
        /** Any failure; the .iso is untouched. */
        data class Failed(val message: String) : CompressState
    }

    private val _state = MutableStateFlow<CompressState>(CompressState.Idle)
    val state: StateFlow<CompressState> = _state.asStateFlow()

    fun dismiss() { _state.value = CompressState.Idle }

    /** Compress the ISO at [launchUri] into a verified .zar beside it, then ASK whether to
     *  delete the .iso — and only then. */
    fun compress(launchUri: String) = viewModelScope.launch {
        _state.value = CompressState.Busy("Compressing…", 0f)
        // Concurrent read is safe: the progress getter reads file-static atomics.
        val poll = launch {
            while (isActive) {
                val p = EmulatorRuntime.emulator?.compressProgress() ?: 0f
                (_state.value as? CompressState.Busy)
                    ?.takeIf { it.message == "Compressing…" }
                    ?.let { _state.value = it.copy(progress = p.coerceIn(0f, 1f)) }
                delay(200)
            }
        }
        val result = withContext(Dispatchers.IO) { runCompress(launchUri) }
        poll.cancel()
        _state.value = result
    }

    fun deleteIso() = viewModelScope.launch {
        val s = _state.value as? CompressState.ConfirmDelete ?: return@launch
        val deleted = withContext(Dispatchers.IO) { File(s.isoPath).delete() }
        _state.value = CompressState.Done(
            if (deleted) "Replaced the .iso with “${s.zarName}”."
            else "Created “${s.zarName}”, but the .iso couldn't be deleted. " +
                "You can delete it manually.")
    }

    /** Keep both files. Also the dismiss path, so a stray tap outside the dialog can
     *  never delete anything. */
    fun keepIso() {
        val s = _state.value as? CompressState.ConfirmDelete ?: return
        _state.value = CompressState.Done(
            "Created “${s.zarName}”. The original .iso was kept.")
    }

    private suspend fun runCompress(launchUri: String): CompressState {
        EmulatorRuntime.ensureLoaded()
        val emulator = EmulatorRuntime.emulator
            ?: return CompressState.Failed("Emulator not loaded. .iso is unchanged.")

        // Resolve-only prologue — NOTHING is mutated yet.
        val isoFile = File(launchUri)
        if (!isoFile.isFile)
            return CompressState.Failed("Couldn't open the ISO. .iso is unchanged.")
        val parent = isoFile.parentFile?.takeIf { it.canWrite() }
            ?: return CompressState.Failed("Game folder isn't writable. .iso is unchanged.")

        val isoName = isoFile.name
        val baseName = isoName.removeSuffix(".iso").removeSuffix(".ISO").ifBlank { "game" }
        val zarName = "$baseName.zar"
        if (File(parent, zarName).exists())
            return CompressState.Failed("A “$zarName” already exists in the folder. .iso is unchanged.")

        val tempZar = File.createTempFile("compress", ".zar", appContext.cacheDir)
        tempZar.delete()   // native creates the output; it must not pre-exist
        var placed: File? = null
        try {
            // (1) PACK + VERIFY: native packs the ISO's VFS into the temp .zar, then re-opens
            //     and verifies it. 0 == created AND verified.
            val packStatus = emulator.compressIsoToZar(isoFile.absolutePath, tempZar.absolutePath)
            if (packStatus != 0)
                return CompressState.Failed(
                    "Compression failed (0x${packStatus.toUInt().toString(16)}). .iso is unchanged.")

            // (3) PLACE: copy the verified temp .zar beside the .iso, which stays present.
            _state.value = CompressState.Busy("Replacing…")
            val dest = File(parent, zarName)
            val copied = runCatching { tempZar.copyTo(dest, overwrite = false) }.isSuccess
            if (!copied) {
                dest.delete(); placed = null
                return CompressState.Failed("Couldn't write the .zar. .iso is unchanged.")
            }
            placed = dest
            // (3b) sanity: non-empty and its size matches the temp.
            if (dest.length() <= 0L || dest.length() != tempZar.length()) {
                dest.delete(); placed = null
                return CompressState.Failed("The written .zar looks incomplete. .iso is unchanged.")
            }

            // (4) ASK before deleting the .iso — and only now. A verified .zar is already in
            //     the folder, so either answer leaves the game playable.
            return CompressState.ConfirmDelete(
                isoPath = isoFile.absolutePath,
                isoName = isoName,
                zarName = zarName,
                isoBytes = isoFile.length(),
            )
        } catch (e: Exception) {
            placed?.delete()   // threw AFTER placing but BEFORE deleting iso -> remove orphan .zar
            return CompressState.Failed("Compression failed: ${e.message}. .iso is unchanged.")
        } finally {
            tempZar.delete()
        }
    }
}
