package xendroid.compose.settings

import android.content.Context
import xendroid.compose.Application
import xendroid.emulator.Emulator
import java.io.File

/** Sources the live global config file and the bundled default template baseline. */
class ConfigStore(private val appContext: Context) {

    /** The live, editable config the emulator reads at boot. */
    fun globalConfigFile(): File = Application.get_global_config_file()

    /** <app_data_dir>/config/<TITLE_ID>.config.toml — exactly the path native
     *  config::LoadGameConfig reads (config.cc:309-311). The app_data_dir is the
     *  parent of the global config file (Application.get_app_data_dir() is
     *  package-private; its public sibling get_global_config_file() lives in it). */
    fun perGameConfigFile(titleId: String): File =
        File(File(globalConfigFile().parentFile, "config"), "$titleId.config.toml")

    /** Open the per-game SPARSE overlay for editing. Bootstraps an EMPTY toml table
     *  when missing (NOT the default template — that would make every key an override,
     *  destroying sparseness). Mirrors [openLive]'s catch-and-retry but seeds "". */
    fun openGameConfig(titleId: String): ConfigHandle {
        val file = perGameConfigFile(titleId)
        ensureEmptyFileExists(file)
        return try {
            ConfigHandle.openFile(file.absolutePath)
        } catch (e: Emulator.ConfigFileException) {
            // Corrupt existing -> reset to an empty table, retry once.
            file.writeText("")
            ConfigHandle.openFile(file.absolutePath)
        }
    }

    /** The config/ subdir is NOT auto-created (only the global parent is, below);
     *  seed an empty file (toml++ parses empty input to an empty table) since
     *  open_config_file throws on a missing file. */
    private fun ensureEmptyFileExists(file: File) {
        if (file.exists()) return
        file.parentFile?.mkdirs()
        file.writeText("")
    }

    /** Open the live config for editing. Bootstraps from the default template if the
     *  file is missing/unparseable (open does NOT auto-create). */
    fun openLive(): ConfigHandle {
        val file = globalConfigFile()
        ensureLiveFileExists(file)
        return try {
            ConfigHandle.openFile(file.absolutePath)
        } catch (e: Emulator.ConfigFileException) {
            // Corrupt/empty file -> reseed from template, retry once.
            file.writeText(defaultTemplateText())
            ConfigHandle.openFile(file.absolutePath)
        }
    }

    /** Open the live config READ-ONLY from a text snapshot. Unlike [openLive], the
     *  returned handle is a STRING handle — closing it via [ConfigHandle.closeString]
     *  frees the native table WITHOUT writing back to disk (close_config_file, the
     *  only disk-write path, would needlessly re-serialize on every read). Use this
     *  for pure reads; use [openLive] when you intend to persist edits. */
    fun openLiveSnapshot(): ConfigHandle {
        val file = globalConfigFile()
        ensureLiveFileExists(file)
        return ConfigHandle.openString(file.readText())
    }

    /** Read-only baseline parsed from the bundled asset template, for modified-from-default diffing. */
    fun openTemplateBaseline(): ConfigHandle =
        ConfigHandle.openString(defaultTemplateText())

    fun defaultTemplateText(): String = templateText
        ?: loadTemplateText().also { templateText = it }

    private var templateText: String? = null

    private fun loadTemplateText(): String =
        appContext.assets.open("config/default_config.toml").use { it.readBytes().toString(Charsets.UTF_8) }

    private fun ensureLiveFileExists(file: File) {
        if (file.exists()) return
        file.parentFile?.mkdirs()
        // Prefer the on-disk template materialized on first run; fall back to the asset.
        val template = Application.get_default_config_file()
        if (template.exists()) template.copyTo(file, overwrite = false)
        else file.writeText(defaultTemplateText())
    }
}
