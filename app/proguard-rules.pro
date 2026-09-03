# kotlinx.serialization: keep the generated serializers + Companion for our
# @Serializable models (Game, GameMetadataCache, GamepadLayoutSchema) so R8 can't
# strip/rename them. The runtime library ships the rest of the rules itself.
-keepattributes *Annotation*, InnerClasses
-keepclassmembers @kotlinx.serialization.Serializable class xendroid.compose.** {
    static ** Companion;
    *** serializer(...);
}
-keep,includedescriptorclasses class xendroid.compose.**$$serializer { *; }

# Gson
-keepattributes Signature
-keepattributes RuntimeVisibleAnnotations
-keepattributes *Annotation*

# Retrofit API models
-keep class xendroid.compose.updater.GithubRelease { *; }
-keep interface xendroid.compose.updater.GithubApi { *; }

# JNI keeps for :emulator-core come from its consumer-rules.pro automatically.
