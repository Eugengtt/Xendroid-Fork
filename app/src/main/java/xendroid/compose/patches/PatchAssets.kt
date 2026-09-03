package xendroid.compose.patches

import android.content.Context
import xendroid.compose.Application
import java.io.File

/** The bundled patch catalog (…/assets/game-patches/). Abstracted so [PatchStore] stays simple. */
interface PatchAssets {
    /** Bundled `.patch.toml` filenames (names only). */
    fun list(): List<String>
    /** UTF-8 text of one bundled patch asset. */
    fun read(fileName: String): String
}

/** AssetManager-backed catalog: reads the bundled `game-patches` assets. */
class AssetPatchAssets(private val context: Context) : PatchAssets {
    override fun list(): List<String> =
        (context.assets.list(DIR) ?: emptyArray()).filter { it.endsWith(".patch.toml") }

    override fun read(fileName: String): String =
        context.assets.open("$DIR/$fileName").use { it.readBytes().toString(Charsets.UTF_8) }

    companion object { const val DIR = "game-patches" }
}

/** On-device paths for patches. */
object PatchPaths {
    /** `<app_data_dir>/patches` — the dir the emulator scans (parent of the global config file). */
    fun patchesDir(): File = File(Application.get_global_config_file().parentFile, "patches")
}
