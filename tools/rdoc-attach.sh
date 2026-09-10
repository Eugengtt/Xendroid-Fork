#!/usr/bin/env bash
# Forward a host port to the compose :emu render process's RenderDoc target
# control socket, so qrenderdoc's "Attach to Running Instance" lands on the
# Vulkan renderer instead of the launcher process (which shows "API = None").
#
# Both xendroid.compose.free (launcher) and xendroid.compose.free:emu (renderer) create a
# Vulkan instance with the bundled VK_LAYER_RENDERDOC_Capture layer, so each
# opens its own @renderdoc_3892X target-control socket. qrenderdoc auto-grabs
# 38920, often the wrong one. This script identifies which socket :emu owns and
# forwards HOST_PORT -> that socket.
#
# Usage:
#   1. Set vulkan_renderdoc_capture = true in the device config, launch the game.
#   2. ./tools/rdoc-attach.sh
#   3. In qrenderdoc: File > Attach to Running Instance > add Remote Host
#      "localhost" (NOT the Android device, which would re-forward to the wrong
#      process) > connect. The single target on HOST_PORT is :emu. Wait for a
#      presented frame (API flips to Vulkan), then capture.
set -euo pipefail

# run-as/simpleperf need the debuggable variant; override with XENDROID_PKG.
PKG="${XENDROID_PKG:-xendroid.compose.debug}"
EMU_PROC="${PKG}:emu"
HOST_PORT="${1:-38920}"

# adb server lives on the Windows host in this WSL setup; use the gateway proxy.
export ADB_SERVER_SOCKET="tcp:$(ip route show default | awk '{print $3}'):5037"
ADB="$HOME/Android/Sdk/platform-tools/adb"
SERIAL="$($ADB devices | awk 'NR>1 && $2=="device"{print $1; exit}')"
[ -n "$SERIAL" ] || { echo "No device in 'adb devices'."; exit 1; }
A() { $ADB -s "$SERIAL" "$@"; }

EMU_PID="$(A shell pidof "$EMU_PROC" | tr -d '\r' | awk '{print $1}')"
[ -n "$EMU_PID" ] || { echo "Render process '$EMU_PROC' not running - launch a game first."; exit 1; }
echo ":emu PID = $EMU_PID"

# Listening RenderDoc sockets (St field == 01) with their inodes.
# /proc/net/unix columns: Num RefCount Proto Flags Type St Inode Path
mapfile -t RDOC < <(A shell cat /proc/net/unix 2>/dev/null \
  | tr -d '\r' \
  | awk '$8 ~ /@renderdoc_/ && $6=="01" {print $7" "$8}')
[ "${#RDOC[@]}" -gt 0 ] || { echo "No @renderdoc_* listening sockets - is vulkan_renderdoc_capture=true and the game running?"; exit 1; }
echo "RenderDoc listening sockets:"; printf '  inode=%s name=%s\n' $(printf '%s\n' "${RDOC[@]}")

# Inodes of sockets owned by the :emu process (same app UID -> run-as can read
# its fds). The whole remote command must reach the device shell as ONE quoted
# argument, else `adb shell` word-splits the for-loop ("unexpected do"). Pass a
# single double-quoted string with a single-quoted sh -c body; \$f stays literal
# for the device shell while $PKG/$EMU_PID expand locally.
EMU_INODES="$(A shell "run-as $PKG sh -c 'for f in /proc/$EMU_PID/fd/*; do readlink \$f; done 2>/dev/null'" \
  | tr -d '\r' | grep -o 'socket:\[[0-9]*\]' | grep -o '[0-9]*' | sort -u || true)"

TARGET_SOCK=""
for entry in "${RDOC[@]}"; do
  inode="${entry%% *}"; name="${entry##* }"; name="${name#@}"
  if printf '%s\n' "$EMU_INODES" | grep -qx "$inode"; then
    TARGET_SOCK="$name"; echo "-> :emu owns $name (inode $inode)"; break
  fi
done

# No reliable guess - the launcher can hold multiple sockets (e.g. it re-creates
# its instance), so port order is not deterministic. Require the fd match.
if [ -z "$TARGET_SOCK" ]; then
  echo "ERROR: could not map any @renderdoc socket to :emu (PID $EMU_PID)." >&2
  echo "  :emu socket inodes were:" >&2
  printf '%s\n' "$EMU_INODES" | sed 's/^/    /' >&2
  echo "  Check 'run-as $PKG' access to /proc/$EMU_PID/fd." >&2
  exit 1
fi

# Clear any stale RenderDoc forwards (qrenderdoc auto-creates ones pointing at
# the wrong socket) so only our :emu forward remains.
A forward --list | tr -d '\r' | awk '/renderdoc/{print $2}' | while read -r p; do
  [ -n "$p" ] && A forward --remove "$p" 2>/dev/null || true
done
A forward "tcp:$HOST_PORT" "localabstract:$TARGET_SOCK"
echo
echo "Forwarded host tcp:$HOST_PORT -> device localabstract:$TARGET_SOCK (:emu renderer)"
echo "qrenderdoc: File > Attach to Running Instance > Remote Host 'localhost' > connect."
echo "Pick the target on port $HOST_PORT, wait for a presented frame (API -> Vulkan), capture."
A forward --list | sed 's/^/  /'
