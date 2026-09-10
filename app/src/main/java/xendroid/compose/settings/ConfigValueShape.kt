package xendroid.compose.settings

/**
 * Pure (JNI-free) helpers encoding the native `save_config_entry` type-inference
 * contract: the native side re-infers the TOML type from the string shape, so every
 * put must emit the canonical shape and every read must tolerate the round-tripped
 * shapes. Extracted here so the contract is unit-testable without the JNI boundary.
 */
object ConfigValueShape {
    fun bool(v: Boolean) = if (v) "true" else "false"
    fun int(v: Int) = v.toString()

    /** Always include a '.' so the value round-trips as a TOML double, never an int. */
    fun double(v: Double): String { val s = v.toString(); return if (s.contains('.')) s else "$s.0" }

    fun parseBool(raw: String?, def: Boolean) = when (raw) { "true" -> true; "false" -> false; else -> def }

    /** Native ints come back via std::to_string; tolerate a value that round-tripped
     *  as a double (e.g. "8.0"). */
    fun parseInt(raw: String?, def: Int) = raw?.toIntOrNull() ?: raw?.toDoubleOrNull()?.toInt() ?: def
}
