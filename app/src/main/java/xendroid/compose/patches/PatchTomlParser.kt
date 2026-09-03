package xendroid.compose.patches

/**
 * Display-only reader for .patch.toml header fields. Never reserializes (the files carry
 * comments and memory-write tables we must preserve — see [PatchTomlEditor]); reads only the
 * header keys and, per `[[patch]]`, name/desc/author/is_enabled. Sub-tables are ignored.
 */
object PatchTomlParser {

    fun parse(fileName: String, text: String): PatchFile? {
        var titleName = ""
        var titleId: String? = null
        var hashes: List<String> = emptyList()

        val entries = mutableListOf<PatchEntry>()
        var inPatch = false
        var curName = ""
        var curDesc: String? = null
        var curAuthor: String? = null
        var curEnabled = false

        fun flush() {
            if (inPatch) {
                entries.add(
                    PatchEntry(entries.size, curName, curDesc, curAuthor, curEnabled)
                )
            }
            curName = ""; curDesc = null; curAuthor = null; curEnabled = false
        }

        for (raw in text.split("\n")) {
            val line = raw.trim()
            // Prefix match: headers may carry a trailing comment; `[[patch.` sub-tables don't match.
            if (line.startsWith("[[patch]]")) {
                flush()
                inPatch = true
                continue
            }
            if (line.startsWith("[[patch.") || line.startsWith("[patch")) continue
            if (!line.contains('=')) continue
            val key = line.substringBefore('=').trim()
            val rhs = line.substringAfter('=').trim()
            if (!inPatch) {
                when (key) {
                    "title_name" -> titleName = parseString(rhs)
                    "title_id" -> titleId = parseString(rhs)
                    "hash" -> hashes = parseStringOrArray(rhs)
                }
            } else {
                when (key) {
                    "name" -> curName = parseString(rhs)
                    "desc" -> curDesc = parseString(rhs)
                    "author" -> curAuthor = parseString(rhs)
                    "is_enabled" -> curEnabled = rhs.startsWith("true")
                }
            }
        }
        flush()

        val id = titleId ?: return null
        return PatchFile(
            fileName = fileName,
            titleName = titleName,
            titleId = id,
            hashes = hashes,
            variantLabel = variantLabel(fileName, titleName),
            entries = entries,
        )
    }

    /** `"foo"` -> `foo`; tolerates a trailing inline `# comment`. */
    private fun parseString(rhs: String): String {
        val s = rhs.trim()
        if (s.startsWith("\"")) {
            val end = s.indexOf('"', 1)
            if (end > 0) return unescape(s.substring(1, end))
        }
        // Unquoted fallback: take up to a comment marker.
        return s.substringBefore('#').trim()
    }

    /** `"a"` -> [a]; `["a", "b"]` -> [a, b]. */
    private fun parseStringOrArray(rhs: String): List<String> {
        val s = rhs.trim()
        if (!s.startsWith("[")) return listOf(parseString(s)).filter { it.isNotEmpty() }
        return Regex("\"([^\"]*)\"").findAll(s).map { it.groupValues[1] }.toList()
    }

    private fun unescape(s: String): String = s.replace("\\\"", "\"").replace("\\\\", "\\")

    /** Filename `<TITLEID> - <rest>.patch.toml` -> `<rest>`; fallback to title name / base. */
    private fun variantLabel(fileName: String, titleName: String): String {
        val base = fileName.removeSuffix(".patch.toml")
        val dash = base.indexOf(" - ")
        val rest = if (dash >= 0) base.substring(dash + 3).trim() else ""
        return when {
            rest.isNotEmpty() -> rest
            titleName.isNotEmpty() -> titleName
            else -> base
        }
    }
}
