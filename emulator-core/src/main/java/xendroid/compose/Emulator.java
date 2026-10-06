// SPDX-License-Identifier: WTFPL
package xendroid.compose;

import android.content.Context;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.Base64;

public class Emulator extends xendroid.emulator.Emulator{
    public static Emulator get=null;
    public static void load_library(){
        if(get!=null)
            throw new RuntimeException("Emulator already loaded");
        get=new Emulator();
        System.loadLibrary("e");
    }

    /*public void key_event(int keycode,boolean pressed){
        throw new RuntimeException("Not implemented");
        final int unused=-1;
        super.key_event(keycode,pressed,unused);
    }*/

    public native void setup_context(Context ctx);
    public native void setup_launch_args(String[] args);
    public  native void setup_uri_info_list_file(String path);
    public native String simple_device_info();
    public native String generate_config_xml(String config_path);
    // Debug overlay text (FPS / frame time / compile count), or null when the
    // "Display|show_debug_overlay" setting is off. Polled from EmulatorActivity.
    public native String debug_overlay_text();
    // Last presented guest-frame interval, in ms (raw present-to-present delta,
    // NOT averaged). 0 before the first present / right after a pause.
    public native double last_frame_time_ms();
    // Instant fps = 1000/last_frame_time_ms (0 when no frame timed yet). Distinct
    // from the smoothed average fps embedded in debug_overlay_text().
    public native double instant_fps();
    // RenderDoc-style average fps over a ~1s sliding window (0 before first present).
    public native double average_fps();
    // Effective Display|show_debug_overlay (the live cvar, with any per-game config
    // overlay applied by LoadGameConfig at boot). Poll post-boot: the per-game override
    // lands on the detached boot thread, so this only reflects it after the game loads.
    public native boolean show_debug_overlay_enabled();
    public native boolean show_touch_overlay_enabled();
    public native void set_show_touch_overlay(boolean value);

    // Mount the ISO at isoPath (a REAL host ISO path == game.launchUri in
    // real-path mode), walk its filesystem and pack it into a VERIFIED .zar at
    // outZarPath (also a real host path, e.g. beside the .iso). Returns X_STATUS
    // (0 == success, non-zero == failure). The native side only reports success
    // after the .zar re-opens and matches the source disc (file-count + total
    // bytes); on any failure the partial .zar is removed and the caller MUST keep
    // the ISO. MUST be called off the main thread (blocking VFS walk + zstd
    // compress + verify): dispatch from a background coroutine (Dispatchers.IO).
    public native int compressIsoToZar(String isoPath,String outZarPath);

    // Fraction 0..1 of the ISO->.zar compression currently running on another
    // thread (0 when none is in flight). Poll it for a determinate progress bar.
    public native float compressProgress();

    // Extract an STFS/SVOD content package at srcPath into the content tree rooted
    // at contentRoot (both REAL host paths). Placement is content-type aware: profiles
    // under their account XUID, everything else under machine XUID 0. Returns X_STATUS
    // (0 == success). Blocking VFS walk -- MUST be called off the main thread.
    public native int install_content(String srcPath, String contentRoot);
    public native DiscContentItem[] list_disc_content(String discPath);
    public native int install_disc_content(String discPath, String innerPath,
                                           String contentRoot, String scratchDir);

    // Fraction 0..1 of the in-flight content install (0 when none). Poll for a bar.
    public native float installProgress();

    // Header-only probe of an STFS/SVOD package: title id, content type, size,
    // display name, icon. Returns null for a non-CON/unreadable container. Off-main.
    public native ContentInfo content_header(String srcPath);

    // List installed packages of contentType (DLC 0x2, Title Update 0xB0000) for
    // titleId (8-char hex) under contentRoot. Both types live under machine XUID 0.
    // Returns an empty array when nothing is installed. Off-main.
    public native Emulator.ContentItem[] list_content(String contentRoot, String titleId, int contentType);

    // Delete one installed package of contentType (data dir + .header sidecar).
    // Returns X_STATUS (0 == success). Off-main.
    public native int delete_content(String contentRoot, String titleId, int contentType, String pkgDir);

    // Create a kernel-free Xbox-360 profile (encrypted Account blob) under contentRoot.
    // gamertag must be a valid gamertag (1-15 chars). Returns the new XUID as 16-char
    // hex, or null on failure (invalid gamertag / IO error). Off-main.
    public native String create_profile(String contentRoot, String gamertag, int language, int country);

    // Enumerate readable profiles under contentRoot. Returns an empty array when none.
    // Off-main (filesystem walk + per-profile decrypt).
    public native ProfileInfo[] list_profiles(String contentRoot);

    // Update an existing profile's gamertag/language/country (xuid = 16-char hex).
    // Returns X_STATUS (0 == success). Off-main.
    public native int rename_profile(String contentRoot, String xuid, String gamertag, int language, int country);

    // ---- Guest text entry. Blocks a kernel dispatch thread until answered.
    public native KeyboardRequest keyboard_request();
    public native void keyboard_submit(long id, boolean accepted, String text);
    // Fail every waiting request.
    public native void keyboard_cancel_all();

    public native MessageBoxRequest msgbox_request();
    public native void msgbox_submit(long id, int button);
    public native void msgbox_cancel_all();

    public native DiscSwapRequest disc_request();
    public native void disc_submit(long id, boolean accepted, String path);
    public native void disc_cancel_all();
    /** Discs the host offers for this title; set before boot. */
    public native void disc_set_known(String[] labels, String[] paths);

    // ---- Real-path (All Files Access) scan probes: take an ABSOLUTE host path
    // (no Context: no ContentResolver / fd). The core's real-path DiscImageDevice /
    // DiscZarchiveDevice / Extract*Metadata back these. format: 0=ISO, 1=XEX_FOLDER,
    // 2=ZAR (== TID_FMT_* in C++, == GameFormat.titleIdCode). GameInfo.uri is echoed
    // back as the input abs path.

    // Header-only title-id read for ISO / XEX_FOLDER / ZAR from a real path. path = the ISO
    // file (ISO), default.xex (XEX_FOLDER) or .zar file (ZAR). 8-char uppercase-hex id, or
    // null for unsupported/unreadable/00000000.
    public native String   title_id_from_path(String path,int format) throws RuntimeException;

    // Combined name+icon(+titleId) read for ISO / XEX_FOLDER / ZAR from a real path (one XEX
    // decompress + XDBF/SPA parse). Returns a GameInfo (fields may individually be null), or
    // null when nothing was readable. MUST run off the main thread.
    public native GameInfo meta_from_path(String path,int format)     throws RuntimeException;

    // GOD container header read from a real path: title_name + icon + title id. Returns a
    // GameInfo, or null for a non-GOD/unreadable container. MUST run off the main thread.
    public native GameInfo meta_info_from_god_path(String path)       throws RuntimeException;


    public static class GameInfo{

        public String uri;
        public String name;
        public String titleId;   // 8-char uppercase hex, or null for non-GOD / unreadable
        public String mediaId;   // 8-char uppercase hex, or null when unreadable
        public int discNumber;   // 1-based; 0 when the header does not state it
        public int discCount;    // 0 when the header does not state it
        public int fd;
        public byte[] icon;


        static JSONObject to_json(GameInfo  info) throws JSONException {
            JSONObject json=new JSONObject();

            json.put("uri",info.uri);
            if(info.name!=null)
                json.put("name",info.name);
            if(info.titleId!=null)
                json.put("titleId",info.titleId);
            if(info.mediaId!=null)
                json.put("mediaId",info.mediaId);

            if(info.icon!=null)
                json.put("icon", Base64.getEncoder().encodeToString(info.icon));
            return json;
        }

        static GameInfo from_json(JSONObject json) throws JSONException {
            GameInfo info=new GameInfo();
            info.uri=json.getString("uri");
            if(json.has("name"))
                info.name=json.getString("name");
            if(json.has("titleId"))
                info.titleId=json.getString("titleId");
            if(json.has("mediaId"))
                info.mediaId=json.getString("mediaId");
            if(json.has("icon"))
                info.icon=Base64.getDecoder().decode(json.getString("icon"));

            return info;
        }
    }

    // Parsed STFS/SVOD content-package header (CON/LIVE/PIRS). Distinct from
    // GameInfo because the content installer needs contentType, which the GameInfo
    // bridge drops -- without it Kotlin can't tell a DLC CON from a game CON.
    public static class ContentInfo {
        public int titleId;       // raw u32 (compare uppercase-hex against the game's)
        public int contentType;   // XContentType; DLC == 0x2 (kMarketplaceContent)
        public long contentSize;  // payload bytes (be u64 from the metadata header)
        public String displayName;
        public byte[] icon;
    }

    public static class ContentItem {
        public String pkgDir;
        public String displayName;
        public long size;
    }

    /** An installable package found inside a disc image, from list_disc_content. */
    public static class DiscContentItem {
        public String innerPath;
        public String displayName;
        public int titleId;
        public int contentType;
        public long size;
    }

    public static class ProfileInfo {
        public String xuid;       // 16-char uppercase hex
        public String gamertag;
        public int language;      // XLanguage
        public int country;       // XOnlineCountry
        public boolean hasAvatar; // tile_64.png present
    }

    /** A pending guest text-entry prompt. */
    public static class KeyboardRequest {
        public long id;            // stale ids are ignored
        public String title;
        public String description;
        public String defaultText; // clamped to maxLength
        public int maxLength;      // UTF-16 code units, terminator excluded
        public int flags;          // raw guest flags
    }

    /** A pending guest message box (XamShowMessageBoxUI). */
    public static class MessageBoxRequest {
        public long id;           // stale ids are ignored
        public String title;
        public String text;
        public String[] buttons;  // the guest's own labels, at least one
        public int activeButton;  // index the guest pre-focused
        public int flags;         // raw guest flags
    }

    /** A pending guest disc-swap prompt. */
    public static class DiscSwapRequest {
        public long id;              // stale ids are ignored
        public String message;       // the guest's instruction text
        public boolean isError;      // a retry after a rejected disc
        public int discNumber;       // the disc the guest asked for, 1-based
        public String[] discLabels;  // parallel to discPaths
        public String[] discPaths;   // absolute host paths
    }
}
