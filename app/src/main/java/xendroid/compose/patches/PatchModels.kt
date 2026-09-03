package xendroid.compose.patches

/** One `[[patch]]` block; [index] is its 0-based ordinal among blocks (the toggle key). */
data class PatchEntry(
    val index: Int,
    val name: String,
    val desc: String?,
    val author: String?,
    val isEnabled: Boolean,
)

/** A parsed .patch.toml: header + entries. [variantLabel] groups the files of one title. */
data class PatchFile(
    val fileName: String,
    val titleName: String,
    val titleId: String,
    val hashes: List<String>,
    val variantLabel: String,
    val entries: List<PatchEntry>,
)
