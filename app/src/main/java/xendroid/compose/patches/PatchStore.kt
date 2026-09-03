package xendroid.compose.patches

import java.io.File

/**
 * Reads effective patch state and writes per-patch toggles. Effective state = the on-disk file
 * in [patchesDir] if present, else the bundled asset (which ships all-disabled). A toggle copies
 * the asset to disk on first edit, then flips one `is_enabled` line; the emulator reads
 * [patchesDir] on the next launch.
 */
class PatchStore(
    private val assets: PatchAssets,
    private val patchesDir: File,
) {
    /** Files whose 8-hex filename prefix matches [titleId] (case-insensitive). A title may have several. */
    fun patchesForTitle(titleId: String): List<PatchFile> =
        assets.list()
            .filter { it.length >= 8 && it.substring(0, 8).equals(titleId, ignoreCase = true) }
            .sorted()
            .mapNotNull { name -> PatchTomlParser.parse(name, effectiveText(name)) }

    /** Toggle a single `[[patch]]` entry by its 0-based ordinal. */
    fun setEnabled(fileName: String, patchIndex: Int, enabled: Boolean) {
        patchesDir.mkdirs()
        val onDisk = File(patchesDir, fileName)
        val current = if (onDisk.exists()) onDisk.readText() else assets.read(fileName)
        onDisk.writeText(PatchTomlEditor.setEnabled(current, patchIndex, enabled))
    }

    private fun effectiveText(fileName: String): String {
        val onDisk = File(patchesDir, fileName)
        return if (onDisk.exists()) onDisk.readText() else assets.read(fileName)
    }
}
