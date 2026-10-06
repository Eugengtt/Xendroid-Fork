# Building xendroid

xendroid is an Android (arm64-v8a only) build of a xenia-canary fork with an
ARM64 JIT backend. It builds the same way from a **Linux** or **Windows** host —
both cross-compile to Android via the NDK; nothing is built for the host CPU.

The Gradle build produces two modules:

- `:emulator-core` — the native build (CMake -> `libe.so` +
  `libhardware_ProcessorInfo.so`) plus the JNI-bound Java classes. Built once,
  consumed transitively.
- `:app` — the Kotlin/Jetpack-Compose frontend (`applicationId
  compose.compose`).

## Toolchain

| Tool        | Version            | Notes |
|-------------|--------------------|-------|
| JDK         | 21                 | `JAVA_HOME` must point at a JDK 21. Android Studio's bundled JBR (recent Studio = JBR 21) qualifies. |
| Android SDK | platform 35        | `sdk.dir` in `local.properties`. |
| Android NDK | 29.0.14206865      | Pinned in `emulator-core/build.gradle` (`ndkVersion`). |
| CMake       | 3.30.3             | Pinned in `emulator-core/build.gradle` (`cmake { version '3.30.3' }`) and selected via `cmake.dir`. Uses Ninja. |
| Gradle      | 8.11.1             | Provided by the wrapper (`./gradlew` / `gradlew.bat`); the distribution is SHA-256 pinned in `gradle/wrapper/gradle-wrapper.properties`. |
| Python      | 3.x                | Runs on the **build host**; drives the shader compile step. |
| SPIR-V tools| glslang + SPIRV-Tools | `glslangValidator`, `spirv-opt`, `spirv-dis` on the build host (or under `$VULKAN_SDK/bin`). Validated at configure time. |

## Prerequisites

### Linux host

```bash
# JDK 21 + Python + the SPIR-V shader toolchain
sudo apt install openjdk-21-jdk python3 glslang-tools spirv-tools
export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64   # adjust to your distro

# Android SDK pieces (via Android Studio, or cmdline-tools' sdkmanager):
sdkmanager "platform-tools" "platforms;android-35" "ndk;29.0.14206865" "cmake;3.30.3"
```

### Windows host

1. **JDK 21** — install Microsoft OpenJDK or Temurin 21 (or reuse Android
   Studio's JBR at `<AndroidStudio>\jbr`). Set it (open a *new* shell after):
   ```
   setx JAVA_HOME "C:\Program Files\Microsoft\jdk-21"
   ```
2. **Android SDK / NDK / CMake** — install Android Studio, or with cmdline-tools:
   ```
   sdkmanager.bat "platform-tools" "platforms;android-35" "ndk;27.2.12479018" "cmake;3.30.3"
   ```
3. **SPIR-V shader toolchain** — install the **LunarG Vulkan SDK**
   (<https://vulkan.lunarg.com/sdk/home#windows>, or `winget install
   KhronosGroup.VulkanSDK`). The installer sets `VULKAN_SDK` machine-wide and
   provides `glslangValidator.exe`, `spirv-opt.exe`, `spirv-dis.exe` in
   `%VULKAN_SDK%\Bin`. The **SDK** (not just the runtime) is required.
4. **Python 3** — from <https://python.org> or `winget install
   Python.Python.3.12`. Use the python.org installer, **not** the Microsoft
   Store stub, and confirm `py -3 --version`.
5. **Enable long paths** (the vendored xenia third_party tree is deep — see
   [Windows notes](#windows-notes) below).

## First-time setup

0. **Initialize the xenia third_party submodules.** As of the xenia-edge rebase,
   `emulator-core/src/main/cpp/xenia-canary/third_party/` is a set of **git
   submodules** (matching upstream edge) rather than vendored files. Populate them
   before building:
   ```bash
   git submodule update --init --recursive
   ```
   A fresh clone needs `git clone --recurse-submodules`, or this command afterwards.
   (Submodule paths are declared in the repo-root `.gitmodules`, prefixed with
   `emulator-core/src/main/cpp/xenia-canary/`.)

1. Copy the example config and edit the paths:

   ```bash
   cp local.properties.example local.properties     # Windows: copy local.properties.example local.properties
   ```

   Set `sdk.dir` and `cmake.dir`. **On Windows use forward slashes** (a backslash
   is an escape char in `.properties` files):
   ```properties
   sdk.dir=C:/Users/you/AppData/Local/Android/Sdk
   cmake.dir=C:/Users/you/AppData/Local/Android/Sdk/cmake/3.30.3
   ```

2. **CMake shadowing gotcha.** The native build must use the SDK's CMake
   3.30.3. A different `cmake` earlier on `PATH` (a pip-installed
   `~/.local/bin/cmake`, or a distro `/usr/bin/cmake` 4.x) can shadow it and
   break the configure step. Pointing `cmake.dir` at `$SDK/cmake/3.30.3` makes
   Gradle use the right one regardless of `PATH`.

3. **Do not configure the native CMake project standalone.** The native build is
   only valid when driven by Gradle, which injects the Android toolchain, the
   `arm64-v8a` ABI, the NDK, and `cmake.dir`. Opening
   `emulator-core/src/main/cpp/CMakeLists.txt` directly in an IDE's CMake
   integration (CLion / Visual Studio / VS Code CMake Tools) configures it with
   the **host** compiler and fails (e.g. `nmake -? failed / CMAKE_C_COMPILER not
   set`). In an IDE, import the **Gradle project**, not the CMakeLists.

## Generated shaders

The GPU/UI Vulkan shaders live as `.xesl`/`.xesli` GLSL sources under
`emulator-core/src/main/cpp/xenia-canary/src/xenia/{gpu,ui}/shaders/`. The CMake
build compiles them to SPIR-V C headers (`bytecode/vulkan_spirv/*.h`) as a
pre-build step, via `tools/build/compile_shader_spirv.py`
(`glslangValidator` -> `spirv-opt` -> `spirv-dis` -> `.h`). This is wired
through `xe_shader_rules_spirv()` in `cmake/XeniaHelpers.cmake` and runs
automatically on every build — the first build generates the headers.

The generated `bytecode/` headers are **not** checked in (they are
`.gitignore`d); they are regenerated whenever a shader source changes. The host
shader tools (above) must therefore be installed — configure fails fast with an
actionable message if any is missing. The tools are found on `PATH`, or set
`VULKAN_SDK` to pin a specific Vulkan SDK (its `bin`/`Bin` is searched).

> The DXBC/FXC path (`compile_shader_dxbc.py`) is D3D12-only and is **not** part
> of the Android build — Windows contributors do not need the Windows SDK or
> `fxc.exe`.

## Building

```bash
# Linux:
./gradlew :app:assembleDebug

# Windows (cmd.exe / PowerShell):
gradlew.bat :app:assembleDebug

# Clean native + app, then build (Linux shown; use gradlew.bat on Windows):
./gradlew clean :app:assembleDebug

# Install to a connected device:
./gradlew :app:installDebug
```

First build downloads Gradle 8.11.1 (SHA-256 verified) and compiles the full
native tree — ~8–9 min cold. The APK lands in
`app/build/outputs/apk/debug/`.

## Windows notes

The vendored `xenia-canary/third_party` tree is deep; combined with Gradle's
`.cxx` intermediate dirs, object paths approach the legacy 260-char `MAX_PATH`
limit. Before cloning on Windows:

```
git config --global core.longpaths true
```

Also enable Win32 long paths OS-wide (Group Policy "Enable Win32 long paths", or
registry `HKLM\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled=1`)
so the CMake/Ninja toolchain itself can create long object paths, and clone into
a **short root** (e.g. `C:\dev\xendroid`, not under `Documents`).

## Corporate TLS interception (Zscaler etc.)

If your network re-signs TLS, Gradle's downloads fail certificate validation.
The robust, cross-platform fix is to import the proxy's root cert into the JDK
truststore once (also fixes the NDK/CMake/sdkmanager downloads):

```
keytool -importcert -alias corp-proxy -cacerts -file proxy-root.cer   # password: changeit
```

Or point Gradle at a truststore for the session:

```bash
# Linux:
export GRADLE_OPTS="-Djavax.net.ssl.trustStore=/etc/ssl/certs/java/cacerts \
                    -Djavax.net.ssl.trustStorePassword=changeit"
```
```powershell
# Windows PowerShell:
$env:GRADLE_OPTS = "-Djavax.net.ssl.trustStore=$env:JAVA_HOME\lib\security\cacerts -Djavax.net.ssl.trustStorePassword=changeit"
```
```bat
:: Windows cmd.exe:
set GRADLE_OPTS=-Djavax.net.ssl.trustStore=%JAVA_HOME%\lib\security\cacerts -Djavax.net.ssl.trustStorePassword=changeit
```
