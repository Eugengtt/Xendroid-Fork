# JNI boundary. The native side (libe.so / libhardware_ProcessorInfo.so) resolves
# these classes by FQN via FindClass and reads their fields / constructors by name
# (e.g. Config.n_handle, Path.fd, GameInfo.*). Renaming or removing any member
# breaks JNI at runtime, so keep the whole surface. These are tiny data/native
# declaration classes, so keeping them costs almost no dex.
-keep class xendroid.emulator.Emulator { *; }
-keep class xendroid.emulator.Emulator$* { *; }
-keep class xendroid.compose.Emulator { *; }
-keep class xendroid.compose.Emulator$* { *; }
-keep class xendroid.hardware.ProcessorInfo { *; }
