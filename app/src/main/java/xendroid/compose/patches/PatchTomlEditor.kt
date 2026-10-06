package xendroid.compose.patches

/**
 * Flips one `[[patch]]` block's `is_enabled` line, preserving every other line byte-for-byte
 * (the files rely on comments/exact formatting — never reserialize). [patchIndex] is the
 * 0-based block ordinal, matching [PatchEntry.index].
 */
object PatchTomlEditor {

    private val ENABLED = Regex("^(\\s*is_enabled\\s*=\\s*)(true|false)(.*)$")

    fun setEnabled(text: String, patchIndex: Int, enabled: Boolean): String {
        val lines = text.split("\n").toMutableList()
        var ordinal = -1
        var targetHeaderLine = -1
        for (i in lines.indices) {
            val trimmed = lines[i].trim()
            // Prefix match: tolerate a trailing comment on the header; exclude `[[patch.` sub-tables.
            if (trimmed.startsWith("[[patch]]")) {
                ordinal++
                if (ordinal == patchIndex) targetHeaderLine = i
                continue
            }
            if (ordinal == patchIndex) {
                val m = ENABLED.matchEntire(lines[i])
                if (m != null) {
                    lines[i] = "${m.groupValues[1]}$enabled${m.groupValues[3]}"
                    return lines.joinToString("\n")
                }
            }
        }
        // No is_enabled line in the target block: insert one right after its header.
        if (targetHeaderLine >= 0) {
            lines.add(targetHeaderLine + 1, "is_enabled = $enabled")
        }
        return lines.joinToString("\n")
    }
}
