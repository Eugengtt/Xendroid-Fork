package xendroid.compose.ui.profile

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import xendroid.compose.core.ContentPaths
import xendroid.compose.core.EmulatorRuntime
import xendroid.compose.core.Gamertag
import xendroid.compose.core.ProfilePaths
import xendroid.compose.settings.ConfigStore
import java.io.File

class ProfileManagerViewModel(
    private val appContext: Context,
    private val configStore: ConfigStore,
) : ViewModel() {

    data class ProfileEntry(
        val xuid: String,
        val gamertag: String,
        val language: Int,
        val country: Int,
        val hasAvatar: Boolean,
        val isActive: Boolean,
    )

    sealed interface ListState {
        data object Loading : ListState
        data class Loaded(val profiles: List<ProfileEntry>) : ListState
        data class Error(val message: String) : ListState
    }

    private val _listState = MutableStateFlow<ListState>(ListState.Loading)
    val listState: StateFlow<ListState> = _listState.asStateFlow()

    sealed interface OpState {
        data object Idle : OpState
        data class Busy(val message: String) : OpState
        data class Done(val message: String) : OpState
        data class Failed(val message: String) : OpState
    }

    private val _opState = MutableStateFlow<OpState>(OpState.Idle)
    val opState: StateFlow<OpState> = _opState.asStateFlow()

    init { refresh() }

    fun dismiss() { _opState.value = OpState.Idle }

    fun refresh() = viewModelScope.launch {
        _listState.value = ListState.Loading
        _listState.value = withContext(Dispatchers.IO) {
            EmulatorRuntime.ensureLoaded()
            val emu = EmulatorRuntime.emulator
                ?: return@withContext ListState.Error("Emulator not loaded.")
            val root = ContentPaths.contentRoot().absolutePath
            try {
                val active = activeXuid()
                val profiles = emu.list_profiles(root)?.map {
                    ProfileEntry(
                        xuid = it.xuid,
                        gamertag = it.gamertag ?: "",
                        language = it.language,
                        country = it.country,
                        hasAvatar = it.hasAvatar,
                        isActive = it.xuid.equals(active, ignoreCase = true),
                    )
                }?.sortedBy { it.gamertag.lowercase() }
                    ?: return@withContext ListState.Error("Couldn't read profiles.")
                ListState.Loaded(profiles)
            } catch (t: RuntimeException) {
                ListState.Error(t.message ?: "Couldn't read profiles.")
            }
        }
    }

    fun create(gamertag: String, language: Int, country: Int, avatarUri: Uri?) = viewModelScope.launch {
        if (!Gamertag.isValid(gamertag)) {
            _opState.value = OpState.Failed("Enter a valid gamertag (1-15 characters).")
            return@launch
        }
        _opState.value = OpState.Busy("Creating profile…")
        val result = withContext(Dispatchers.IO) {
            EmulatorRuntime.ensureLoaded()
            val emu = EmulatorRuntime.emulator ?: return@withContext null
            val xuid = emu.create_profile(
                ContentPaths.contentRoot().absolutePath, gamertag, language, country)
            if (xuid != null && avatarUri != null) writeAvatar(xuid, avatarUri)
            xuid
        }
        if (result != null) {
            _opState.value = OpState.Done("Created “$gamertag”.")
            refresh()
        } else {
            _opState.value = OpState.Failed("Couldn't create the profile.")
        }
    }

    fun rename(xuid: String, gamertag: String, language: Int, country: Int, avatarUri: Uri?) =
        viewModelScope.launch {
            if (!Gamertag.isValid(gamertag)) {
                _opState.value = OpState.Failed("Enter a valid gamertag (1-15 characters).")
                return@launch
            }
            _opState.value = OpState.Busy("Saving…")
            val status = withContext(Dispatchers.IO) {
                EmulatorRuntime.ensureLoaded()
                val emu = EmulatorRuntime.emulator ?: return@withContext -1
                val st = emu.rename_profile(
                    ContentPaths.contentRoot().absolutePath, xuid, gamertag, language, country)
                if (st == 0 && avatarUri != null) writeAvatar(xuid, avatarUri)
                st
            }
            if (status == 0) {
                _opState.value = OpState.Done("Saved “$gamertag”.")
                refresh()
            } else {
                _opState.value = OpState.Failed(renameReasonFor(status))
            }
        }

    fun setActive(xuid: String) = viewModelScope.launch {
        withContext(Dispatchers.IO) { writeActiveXuid(xuid.uppercase()) }
        _opState.value = OpState.Done("Active profile set. Applies on next game launch.")
        refresh()
    }

    fun delete(xuid: String) = viewModelScope.launch {
        _opState.value = OpState.Busy("Removing…")
        val ok = withContext(Dispatchers.IO) {
            val id = xuid.uppercase()
            if (!ProfilePaths.XUID_REGEX.matches(id)) return@withContext false
            val dir = File(ContentPaths.contentRoot(), id)
            val removed = dir.deleteRecursively()
            if (activeXuid().equals(id, ignoreCase = true)) writeActiveXuid("")
            removed
        }
        if (ok) {
            _opState.value = OpState.Done("Profile removed.")
            refresh()
        } else {
            _opState.value = OpState.Failed("Couldn't remove the profile.")
        }
    }

    private fun activeXuid(): String {
        val h = configStore.openLiveSnapshot()
        return try {
            h.getString("Profiles", "logged_profile_slot_0_xuid") ?: ""
        } finally {
            h.closeString()
        }
    }

    private fun writeActiveXuid(xuid: String) {
        val h = configStore.openLive()
        try {
            h.putString("Profiles", "logged_profile_slot_0_xuid", xuid)
        } finally {
            h.closeFile()
        }
    }

    private fun writeAvatar(xuid: String, uri: Uri) {
        val src = appContext.contentResolver.openInputStream(uri)?.use {
            BitmapFactory.decodeStream(it)
        } ?: return
        val square = centerCropSquare(src)
        val dir = ProfilePaths.profileDir(xuid).also { it.mkdirs() }
        writePng(square, 64, File(dir, "tile_64.png"))
        writePng(square, 32, File(dir, "tile_32.png"))
        if (square != src) square.recycle()
        src.recycle()
    }

    private fun centerCropSquare(bmp: Bitmap): Bitmap {
        val side = minOf(bmp.width, bmp.height)
        if (side == bmp.width && side == bmp.height) return bmp
        val x = (bmp.width - side) / 2
        val y = (bmp.height - side) / 2
        return Bitmap.createBitmap(bmp, x, y, side, side)
    }

    private fun writePng(square: Bitmap, size: Int, dest: File) {
        val scaled = Bitmap.createScaledBitmap(square, size, size, true)
        dest.outputStream().use { scaled.compress(Bitmap.CompressFormat.PNG, 100, it) }
        if (scaled != square) scaled.recycle()
    }

    private fun renameReasonFor(status: Int): String = when (status) {
        -1 -> "Emulator not loaded."
        0xC000000D.toInt() -> "Enter a valid gamertag (1-15 characters)."
        0xC0000034.toInt() -> "That profile no longer exists."
        0xC0000022.toInt() -> "Couldn't write the profile files."
        else -> "Save failed (0x${status.toUInt().toString(16)})."
    }
}
