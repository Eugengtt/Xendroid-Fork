# Launching XenDroid from emulation frontends (ES-DE, Daijishō, ...)

XenDroid's emulator activity is exported and boots a game directly from a launch
intent, the same way Dolphin/PPSSPP standalone integrations work. No XenDroid UI
is involved; when the game exits, the emulator process ends and control returns
to the frontend.

## Intent surface

- **Component**: `xendroid.compose/xendroid.compose.EmulatorHostActivity`
- **Action**: `xendroid.intent.action.xendroid` (or `android.intent.action.VIEW`)
- **Game selection**, first match wins:
  1. string extra `game_uri` — absolute path, `file://`, or `content://`
  2. string extra `AutoStartFile` — same formats (the Dolphin convention,
     so Dolphin-style frontend templates can be copied verbatim)
  3. the intent **data URI** (`file://`, `content://`, or a bare path)

`content://` URIs are resolved to a real path where possible
(documents-provider IDs, provider-embedded paths, MediaStore lookup) with an
open-fd passthrough as last resort. **Prefer sending a plain absolute path** —
XenDroid runs in All Files Access real-path mode, so raw paths are the native
format and work for ISO/ZAR/XEX alike.

Command-line test:

```sh
adb shell am start -n xendroid.compose/.EmulatorHostActivity \
  -a xendroid.intent.action.xendroid \
  --es game_uri '/storage/emulated/0/ROMs/xbox360/Game.iso'
```

## ES-DE

ES-DE has no built-in Xbox 360 system on Android, so add a custom system.
Files live in `ES-DE/custom_systems/` on the device.

`es_find_rules.xml`:

```xml
<?xml version="1.0"?>
<ruleList>
  <emulator name="XENDROID">
    <rule type="androidpackage">
      <entry>xendroid.compose/xendroid.compose.EmulatorHostActivity</entry>
    </rule>
  </emulator>
</ruleList>
```

`es_systems.xml`:

```xml
<?xml version="1.0"?>
<systemList>
  <system>
    <name>xbox360</name>
    <fullname>Microsoft Xbox 360</fullname>
    <path>%ROMPATH%/xbox360</path>
    <extension>.iso .ISO .zar .ZAR .xex .XEX</extension>
    <command label="XenDroid (Standalone)">%EMULATOR_XENDROID% %ACTIVITY_CLEAR_TASK% %ACTION%=xendroid.intent.action.xendroid %EXTRA_game_uri%=%ROMRAW%</command>
    <platform>xbox360</platform>
    <theme>xbox360</theme>
  </system>
</systemList>
```

`%ROMRAW%` passes the unescaped absolute path (XenDroid's native format). A
SAF-style variant also works thanks to the content resolver:
`%EXTRA_AutoStartFile%=%ROMSAF%`.

## Daijishō / Beacon

Create a custom player with:

- Package: `xendroid.compose`
- Class/Component: `xendroid.compose.EmulatorHostActivity`
- Action: `xendroid.intent.action.xendroid`
- Extra (string): `game_uri` = `{file.path}` (Daijishō) / the raw path variable
  of the frontend

## Notes for frontend maintainers

- The activity is `singleTask` in its own `:emu` process; the process exits
  when the game closes, so back-to-back launches always cold-boot cleanly.
- One game per process; sending a second launch intent while a game is running
  routes to the existing session (it does not switch games).
- Supported formats: `.iso` (XGD), `.zar`, `.xex`, and STFS containers.
