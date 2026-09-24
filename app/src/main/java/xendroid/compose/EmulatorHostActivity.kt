package xendroid.compose

import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.os.Process
import android.util.Log
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import xendroid.compose.core.EmuProcessLink
import xendroid.compose.core.EmulatorRuntime
import xendroid.compose.core.FrontendLaunch
import xendroid.compose.core.EmulatorSession
import xendroid.compose.core.ScreenBrightnessSampler
import xendroid.compose.core.SessionLogs
import kotlin.math.abs
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.firstOrNull
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import androidx.lifecycle.lifecycleScope
import android.content.res.Configuration
import android.widget.Toast
import android.os.VibrationEffect
import android.os.Vibrator
import android.preference.PreferenceManager
import androidx.activity.compose.BackHandler
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.State
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import kotlin.math.roundToInt
import kotlinx.coroutines.flow.StateFlow
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.viewinterop.AndroidView
import xendroid.compose.ui.disc.DiscSwapPanel
import xendroid.compose.ui.keyboard.GuestKeyboardPanel
import xendroid.compose.ui.keyboard.clampToUtf16Units
import xendroid.compose.ui.messagebox.GuestMessageBoxPanel
import xendroid.compose.ui.pause.PAUSE_OPTION_COUNT
import xendroid.compose.ui.pause.PAUSE_OPTION_QUIT
import xendroid.compose.ui.pause.PAUSE_OPTION_TOUCH_OVERLAY
import xendroid.compose.ui.pause.PAUSE_OPTION_RESUME
import xendroid.compose.ui.panel.GuestSidePanel
import xendroid.compose.ui.pause.PauseMenuPanel
import xendroid.compose.ui.theme.xendroidTheme
import xendroid.compose.gamepad.GamepadConfigDto
import xendroid.compose.gamepad.GamepadController
import xendroid.compose.gamepad.GamepadOverlay
import xendroid.compose.gamepad.Kc
import xendroid.compose.gamepad.rememberAutoHide
import xendroid.compose.data.GameButtons
import xendroid.compose.data.KeymapStore
import xendroid.compose.settings.ConfigStore

/**
 * The :emu emulator host (separate process; see manifest). Reads game_uri from the Intent,
 * performs the PRE-surface native setup, hosts a Vulkan SurfaceView, and drives the exact
 * surface->boot ordering. onDestroy hard-kills the process.
 */
class EmulatorHostActivity : ComponentActivity(), SurfaceHolder.Callback {

    companion object {
        private const val TAG = "EmuHost"
        private const val KEYBOARD_POLL_MS = 150L
        const val EXTRA_GAME_URI = "game_uri"
        const val EXTRA_DISC_LABELS = "disc_labels"
        const val EXTRA_DISC_PATHS = "disc_paths"

        private const val KC_DPAD_LEFT = 0
        private const val KC_DPAD_UP = 1
        private const val KC_DPAD_RIGHT = 2
        private const val KC_DPAD_DOWN = 3
        private const val KC_A = 4
        private const val KC_B = 5
        private const val KC_X = 6
        private const val KC_Y = 7
        private const val KC_BACK = 8
        private const val KC_START = 9
        private const val KC_SHOULDER_L = 10
        private const val KC_SHOULDER_R = 11
        private const val KC_TRIGGER_L = 14
        private const val KC_TRIGGER_R = 15
        private const val KC_LTHUMB_LEFT = 16
        private const val KC_LTHUMB_UP = 17
        private const val KC_LTHUMB_RIGHT = 18
        private const val KC_LTHUMB_DOWN = 19
        private const val KC_RTHUMB_LEFT = 20
        private const val KC_RTHUMB_UP = 21
        private const val KC_RTHUMB_RIGHT = 22
        private const val KC_RTHUMB_DOWN = 23
        private const val KEY_VALUE_UNUSED = -1
        private const val AXIS_DEADZONE = 0.08f
        private const val FOCUS_PAUSE_DEBOUNCE_MS = 250L

        private const val DISPLAY_SETTINGS_PREFS = "display_settings"
        private const val FULLSCREEN_STRETCH_KEY =
            "fullscreen_stretch_enabled"
    }

    private val session = EmulatorSession()
    private var surfaceView: SurfaceView? = null
    private var started = false

    private val mainHandler = Handler(Looper.getMainLooper())
    private val pauseOnFocusLost =
        Runnable {
            if (session.booted) session.pause()
        }

    private val gamepad by lazy {
        GamepadController(applicationContext)
    }

    private val brightnessSampler = ScreenBrightnessSampler()

    @Volatile
    private var overlayWantsBrightness = false

    private var hapticsEnabled = false

    private val bootedState = mutableStateOf(false)
    private val fpsLimitState = mutableIntStateOf(60)
    private val showFpsOverlay = mutableStateOf(false)
    private val performanceOverlayEnabled = mutableStateOf(false)

    // Fullscreen stretch:
    // false = preserve aspect ratio / black bars
    // true  = stretch game image to fill the display
    private val fullscreenStretchEnabled = mutableStateOf(false)

    private val showTouchOverlay = mutableStateOf<Boolean?>(null)
    private val menuOpenState = mutableStateOf(false)

    private val keyboardRequestState =
        mutableStateOf<Emulator.KeyboardRequest?>(null)

    private val discRequestState =
        mutableStateOf<Emulator.DiscSwapRequest?>(null)

    private val messageBoxRequestState =
        mutableStateOf<Emulator.MessageBoxRequest?>(null)

    private val panelSelectedState = mutableIntStateOf(0)
    private val keyboardTextState = mutableStateOf("")

    @Volatile
    private var keyMap: Map<Int, Int> =
        GameButtons.DEFAULT_LOOKUP

    private var vibrator: Vibrator? = null
    private var lTriggerDown = false
    private var rTriggerDown = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterImmersiveMode()

        EmuProcessLink.bindToMainProcess(this)

        val gameUri =
            FrontendLaunch.resolveGamePath(
                this,
                intent
            )

        if (gameUri.isNullOrEmpty()) {
            Log.e(
                TAG,
                "No bootable game in launch intent; finishing"
            )

            Toast.makeText(
                this,
                "XenDroid: no game in launch intent",
                Toast.LENGTH_LONG
            ).show()

            finish()
            return
        }

        if (!EmulatorRuntime.supportsVulkan) {
            Toast.makeText(
                this,
                "No Vulkan GPU; cannot boot",
                Toast.LENGTH_LONG
            ).show()

            finish()
            return
        }

        lifecycleScope.launch {
            val store = KeymapStore(applicationContext)

            keyMap =
                withContext(Dispatchers.IO) {
                    EmulatorRuntime.ensureLoaded()

                    store.androidToGameKey.firstOrNull()
                        ?: GameButtons.DEFAULT_LOOKUP
                }

            showFpsOverlay.value =
                withContext(Dispatchers.IO) {
                    readShowDebugOverlay()
                }

            performanceOverlayEnabled.value =
                getSharedPreferences(
                    "fps_overlay",
                    MODE_PRIVATE
                ).getBoolean(
                    "performance_overlay_enabled",
                    false
                )

            fullscreenStretchEnabled.value =
                getSharedPreferences(
                    DISPLAY_SETTINGS_PREFS,
                    MODE_PRIVATE
                ).getBoolean(
                    FULLSCREEN_STRETCH_KEY,
                    withContext(Dispatchers.IO) {
                        readFullscreenStretch()
                    }
                )

            if (
                Build.VERSION.SDK_INT < Build.VERSION_CODES.R ||
                !Environment.isExternalStorageManager()
            ) {
                Toast.makeText(
                    this@EmulatorHostActivity,
                    "All Files Access not granted; open the library first",
                    Toast.LENGTH_LONG
                ).show()

                finish()
                return@launch
            }

            withContext(Dispatchers.IO) {
                runCatching {
                    SessionLogs.ensureCaptureRunning()
                }.onFailure {
                    Log.w(
                        TAG,
                        "Session log capture failed",
                        it
                    )
                }
            }

            prepareNativeRealPath(gameUri)

            // Re-apply the saved display setting immediately before the
            // emulator session is allowed to boot.
            withContext(Dispatchers.IO) {
                persistFullscreenStretchConfig(
                    fullscreenStretchEnabled.value
                )
            }

            installSurfaceView()
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)

        if (hasFocus) {
            mainHandler.removeCallbacks(
                pauseOnFocusLost
            )

            enterImmersiveMode()

            if (
                session.booted &&
                !menuOpenState.value
            ) {
                session.resumeIfPaused()
            }
        } else {
            mainHandler.removeCallbacks(
                pauseOnFocusLost
            )

            mainHandler.postDelayed(
                pauseOnFocusLost,
                FOCUS_PAUSE_DEBOUNCE_MS
            )
        }
    }

    private fun enterImmersiveMode() {
        window.addFlags(
            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
        )

        WindowCompat.setDecorFitsSystemWindows(
            window,
            false
        )

        WindowInsetsControllerCompat(
            window,
            window.decorView
        ).apply {
            systemBarsBehavior =
                WindowInsetsControllerCompat
                    .BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE

            hide(
                WindowInsetsCompat.Type.systemBars()
            )
        }
    }

    private fun readShowDebugOverlay(): Boolean =
        runCatching {
            val handle =
                ConfigStore(applicationContext)
                    .openLiveSnapshot()

            try {
                handle.getBool(
                    "Display",
                    "show_debug_overlay",
                    false
                )
            } finally {
                handle.closeString()
            }
        }.getOrDefault(false)

    private fun readFullscreenStretch(): Boolean =
        runCatching {
            val handle =
                ConfigStore(applicationContext)
                    .openLiveSnapshot()

            try {
                !handle.getBool(
                    "Display",
                    "present_letterbox",
                    true
                )
            } finally {
                handle.closeString()
            }
        }.getOrDefault(false)

    private fun persistFullscreenStretchConfig(
        enabled: Boolean
    ) {
        runCatching {
            val handle =
                ConfigStore(applicationContext)
                    .openLive()

            try {
                handle.putBool(
                    "Display",
                    "present_letterbox",
                    !enabled
                )
            } finally {
                handle.closeFile()
            }
        }.onFailure {
            Log.w(
                TAG,
                "persisting fullscreen stretch failed",
                it
            )
        }
    }

    private fun prepareNativeRealPath(
        absPath: String
    ) {
        session.setupContext(this)
        session.setupGamePathReal(absPath)

        session.setupLaunchArgs(
            arrayOf(
                "--storage_root=" +
                    Utils.get_storage_root_path(),

                "--config=" +
                    Application
                        .get_global_config_file()
                        .absolutePath,

                "--log_file=" +
                    Utils.get_log_file_path(),

                "--log_append=true",
            )
        )

        session.setupUriInfoListFile(
            Application
                .get_uri_info_list_file()
                .absolutePath
        )
    }

    private fun installSurfaceView() {
        val sv =
            SurfaceView(this).apply {
                isFocusable = true
                isFocusableInTouchMode = true

                holder.addCallback(
                    this@EmulatorHostActivity
                )

                setOnGenericMotionListener { _, ev ->
                    onGenericMotion(ev)
                }
            }

        surfaceView = sv

        val compose =
            ComposeView(this).apply {
                setContent {
                    val cfg by gamepad.config.collectAsState(
                        initial = GamepadConfigDto()
                    )

                    val landscape =
                        resources.configuration.orientation ==
                            Configuration.ORIENTATION_LANDSCAPE

                    val booted by bootedState

                    val controls =
                        remember(
                            cfg,
                            landscape
                        ) {
                            gamepad.controlsFor(
                                cfg,
                                landscape
                            )
                        }

                    val (visible, poke) =
                        rememberAutoHide(
                            cfg.globals.autoHideSeconds
                        )

                    val alpha by animateFloatAsState(
                        if (visible) {
                            cfg.globals.opacity
                        } else {
                            0f
                        },
                        tween(500),
                        label = "padAlpha"
                    )

                    val padVisible =
                        showTouchOverlay.value == true

                    val overlayActive =
                        booted &&
                            cfg.globals.enabled &&
                            padVisible

                    DisposableEffect(
                        overlayActive
                    ) {
                        overlayWantsBrightness =
                            overlayActive

                        if (overlayActive) {
                            brightnessSampler.start(sv)
                        }

                        onDispose {
                            brightnessSampler.stop()
                        }
                    }

                    val contrastState =
                        rememberOverlayContrast(
                            brightnessSampler.brightness
                        )

                    LaunchedEffect(
                        cfg.globals.hapticsEnabled
                    ) {
                        configureHaptics(
                            cfg.globals.hapticsEnabled
                        )
                    }

                    GuestSidePanel(
                        fpsLimit =
                            fpsLimitState.intValue,

                        onFpsLimitChange = { value ->
                            fpsLimitState.intValue =
                                value

                            session.setFpsLimit(
                                value
                            )

                            lifecycleScope.launch(
                                Dispatchers.IO
                            ) {
                                runCatching {
                                    val handle =
                                        ConfigStore(
                                            applicationContext
                                        ).openLive()

                                    handle.putString(
                                        "GPU",
                                        "framerate_limit",
                                        value.toString()
                                    )

                                    handle.closeFile()
                                }
                            }
                        },

                        performanceOverlayEnabled =
                            performanceOverlayEnabled.value,

                        onPerformanceOverlayChange = {
                            enabled ->

                            performanceOverlayEnabled.value =
                                enabled

                            getSharedPreferences(
                                "fps_overlay",
                                MODE_PRIVATE
                            )
                                .edit()
                                .putBoolean(
                                    "performance_overlay_enabled",
                                    enabled
                                )
                                .apply()
                        },

                        fullscreenStretchEnabled =
                            fullscreenStretchEnabled.value,

                        onFullscreenStretchChange = {
                            enabled ->

                            // Store the user's choice first.
                            // The renderer itself will use it
                            // on the next emulator boot.
                            fullscreenStretchEnabled.value =
                                enabled

                            getSharedPreferences(
                                DISPLAY_SETTINGS_PREFS,
                                MODE_PRIVATE
                            )
                                .edit()
                                .putBoolean(
                                    FULLSCREEN_STRETCH_KEY,
                                    enabled
                                )
                                .apply()

                            lifecycleScope.launch(
                                Dispatchers.IO
                            ) {
                                val success =
                                    runCatching {
                                        persistFullscreenStretchConfig(
                                            enabled
                                        )
                                    }.isSuccess

                                if (success) {
                                    withContext(
                                        Dispatchers.Main
                                    ) {
                                        finish()
                                    }
                                }
                            }
                        },

                        // Exit Emulation
                        onExitEmulation = {
                            finish()
                        }
                    ) {
                        Box(
                            Modifier.fillMaxSize()
                        ) {
                            AndroidView(
                                factory = {
                                    sv
                                },
                                modifier =
                                    Modifier.fillMaxSize()
                            )

                            if (
                                booted &&
                                cfg.globals.enabled &&
                                padVisible
                            ) {
                                GamepadOverlay(
                                    controls =
                                        controls,

                                    opacity =
                                        alpha,

                                    contrast = {
                                        contrastState.value
                                    },

                                    onUserInteraction =
                                        poke,

                                    onKeyEvent = {
                                        kc,
                                        pressed,
                                        v ->

                                        if (
                                            pressed &&
                                            v ==
                                                Kc.VALUE_UNUSED
                                        ) {
                                            maybeVibrate()
                                        }

                                        session.keyEvent(
                                            kc,
                                            pressed,
                                            v
                                        )
                                    },

                                    modifier =
                                        Modifier.fillMaxSize(),
                                )
                            }

                            LaunchedEffect(
                                booted
                            ) {
                                if (!booted) {
                                    return@LaunchedEffect
                                }

                                while (isActive) {
                                    showTouchOverlay.value =
                                        session
                                            .showTouchOverlayEnabled()

                                    delay(1000)
                                }
                            }

                            FpsOverlay(
                                session = session,

                                visible =
                                    booted &&
                                        performanceOverlayEnabled.value,

                                modifier =
                                    Modifier.fillMaxSize(),
                            )

                            val keyboardRequest by
                                keyboardRequestState

                            LaunchedEffect(
                                booted
                            ) {
                                if (!booted) {
                                    return@LaunchedEffect
                                }

                                while (isActive) {
                                    if (
                                        keyboardRequestState.value ==
                                            null
                                    ) {
                                        session
                                            .keyboardRequest()
                                            ?.let { req ->

                                                keyboardTextState.value =
                                                    clampToUtf16Units(
                                                        req.defaultText
                                                            .orEmpty(),

                                                        if (
                                                            req.maxLength <=
                                                                0
                                                        ) {
                                                            Int.MAX_VALUE
                                                        } else {
                                                            req.maxLength
                                                        }
                                                    )

                                                panelSelectedState.intValue =
                                                    0

                                                keyboardRequestState.value =
                                                    req
                                            }
                                    }

                                    delay(
                                        KEYBOARD_POLL_MS
                                    )
                                }
                            }

                            keyboardRequest?.let { req ->
                                xendroidTheme {
                                    GuestKeyboardPanel(
                                        request = req,

                                        text =
                                            keyboardTextState.value,

                                        onTextChange = {
                                            keyboardTextState.value =
                                                it
                                        },

                                        selected =
                                            panelSelectedState.intValue,

                                        onAccept = {
                                            text ->
                                            acceptKeyboard(
                                                text
                                            )
                                        },

                                        onCancel = {
                                            cancelKeyboard()
                                        },

                                        modifier =
                                            Modifier.fillMaxSize(),
                                    )
                                }
                            }

                            val messageBoxRequest by
                                messageBoxRequestState

                            LaunchedEffect(
                                booted
                            ) {
                                if (!booted) {
                                    return@LaunchedEffect
                                }

                                while (isActive) {
                                    if (
                                        messageBoxRequestState.value ==
                                            null
                                    ) {
                                        session
                                            .messageBoxRequest()
                                            ?.let { req ->

                                                panelSelectedState.intValue =
                                                    req.activeButton

                                                messageBoxRequestState.value =
                                                    req
                                            }
                                    }

                                    delay(
                                        KEYBOARD_POLL_MS
                                    )
                                }
                            }

                            messageBoxRequest?.let { req ->
                                xendroidTheme {
                                    GuestMessageBoxPanel(
                                        request = req,

                                        selected =
                                            panelSelectedState.intValue,

                                        onChoose = {
                                            button ->
                                            answerMessageBox(
                                                button
                                            )
                                        },

                                        modifier =
                                            Modifier.fillMaxSize(),
                                    )
                                }
                            }

                            val discRequest by
                                discRequestState

                            LaunchedEffect(
                                booted
                            ) {
                                if (!booted) {
                                    return@LaunchedEffect
                                }

                                while (isActive) {
                                    if (
                                        discRequestState.value ==
                                            null
                                    ) {
                                        session
                                            .discRequest()
                                            ?.let { req ->

                                                panelSelectedState.intValue =
                                                    0

                                                discRequestState.value =
                                                    req
                                            }
                                    }

                                    delay(
                                        KEYBOARD_POLL_MS
                                    )
                                }
                            }

                            discRequest?.let { req ->
                                xendroidTheme {
                                    DiscSwapPanel(
                                        request = req,

                                        selected =
                                            panelSelectedState.intValue,

                                        onChoose = {
                                            path ->
                                            chooseDisc(
                                                path
                                            )
                                        },

                                        onCancel = {
                                            cancelDisc()
                                        },

                                        modifier =
                                            Modifier.fillMaxSize(),
                                    )
                                }
                            }
                        }
                    }

                    val menuOpen by
                        menuOpenState

                    val keyboardOpen =
                        keyboardRequestState.value !=
                            null

                    val discOpen =
                        discRequestState.value !=
                            null

                    val messageBoxOpen =
                        messageBoxRequestState.value !=
                            null

                    BackHandler(
                        enabled = keyboardOpen
                    ) {
                        cancelKeyboard()
                    }

                    BackHandler(
                        enabled = discOpen
                    ) {
                        cancelDisc()
                    }

                    BackHandler(
                        enabled = messageBoxOpen
                    ) {
                        messageBoxRequestState.value
                            ?.let {
                                answerMessageBox(
                                    it.activeButton
                                )
                            }
                    }

                    BackHandler(
                        enabled =
                            !menuOpen &&
                                !keyboardOpen &&
                                !discOpen &&
                                !messageBoxOpen
                    ) {
                        panelSelectedState.intValue =
                            PAUSE_OPTION_RESUME

                        menuOpenState.value = true

                        if (session.booted) {
                            session.pause()
                        }
                    }

                    BackHandler(
                        enabled = menuOpen
                    ) {
                        closeMenuAndResume()
                    }

                    if (menuOpen) {
                        xendroidTheme {
                            PauseMenuPanel(
                                selected =
                                    panelSelectedState.intValue,

                                touchOverlayShown =
                                    showTouchOverlay.value == true,

                                onToggleTouchOverlay = {
                                    toggleTouchOverlay()
                                },

                                onResume = {
                                    closeMenuAndResume()
                                },

                                onQuit = {
                                    finish()
                                },

                                modifier =
                                    Modifier.fillMaxSize(),
                            )
                        }
                    }
                }
            }

        setContentView(compose)
        sv.requestFocus()
    }

    private fun configureHaptics(
        globalsFlag: Boolean
    ) {
        val legacy =
            PreferenceManager
                .getDefaultSharedPreferences(this)
                .getBoolean(
                    "enable_vibrator",
                    false
                )

        hapticsEnabled =
            globalsFlag || legacy

        if (
            hapticsEnabled &&
            vibrator == null
        ) {
            vibrator =
                getSystemService(
                    VIBRATOR_SERVICE
                ) as Vibrator
        }
    }

    private fun maybeVibrate() {
        if (!hapticsEnabled) return

        vibrator?.vibrate(
            VibrationEffect.createOneShot(
                25,
                VibrationEffect.DEFAULT_AMPLITUDE
            )
        )
    }

    override fun surfaceCreated(
        holder: SurfaceHolder
    ) {
        if (!started) {
            started = true

            session.attachSurface(
                holder.surface
            )

            try {
                val labels =
                    intent.getStringArrayExtra(
                        EXTRA_DISC_LABELS
                    )

                val paths =
                    intent.getStringArrayExtra(
                        EXTRA_DISC_PATHS
                    )

                if (
                    labels != null &&
                    paths != null &&
                    labels.isNotEmpty()
                ) {
                    session.discSetKnown(
                        labels.toList(),
                        paths.toList()
                    )
                }

                session.bootOnce()
                bootedState.value = true
            } catch (
                t: RuntimeException
            ) {
                Log.e(
                    TAG,
                    "boot failed",
                    t
                )

                finish()
            }
        } else {
            session.attachSurface(
                holder.surface
            )

            if (!menuOpenState.value) {
                session.resumeIfPaused()
            }
        }
    }

    override fun surfaceChanged(
        holder: SurfaceHolder,
        format: Int,
        width: Int,
        height: Int
    ) {
        if (!started) return
        if (width == 0 || height == 0) return

        session.changeSurface(
            width,
            height
        )
    }

    override fun surfaceDestroyed(
        holder: SurfaceHolder
    ) {
        if (!started) return

        if (session.booted) {
            session.pause()
        }

        session.detachSurface()
    }

    override fun onPause() {
        super.onPause()
        session.flushGpuCaches()
    }

    override fun onStop() {
        super.onStop()

        mainHandler.removeCallbacks(
            pauseOnFocusLost
        )

        if (session.booted) {
            session.pause()
        }

        brightnessSampler.stop()

        EmuProcessLink.setEmuForeground(
            false
        )
    }

    override fun onStart() {
        super.onStart()

        EmuProcessLink.setEmuForeground(
            true
        )

        if (overlayWantsBrightness) {
            surfaceView?.let {
                brightnessSampler.start(it)
            }
        }

        if (
            session.booted &&
            !menuOpenState.value
        ) {
            session.resumeIfPaused()
        }
    }

    override fun onDestroy() {
        super.onDestroy()

        keyboardRequestState.value = null
        session.keyboardCancelAll()

        discRequestState.value = null
        session.discCancelAll()

        messageBoxRequestState.value = null
        session.messageBoxCancelAll()

        Process.killProcess(
            Process.myPid()
        )
    }

    override fun onKeyDown(
        keyCode: Int,
        event: KeyEvent
    ): Boolean {
        val nav = panelNav()

        if (nav != null) {
            if (
                panelKeyDown(
                    nav,
                    keyCode
                )
            ) {
                return true
            }

            if (consumeIfGamepad(event)) {
                return true
            }
        }

        val gameKey =
            keyMap[keyCode]
                ?: return consumeIfGamepad(event) ||
                    super.onKeyDown(
                        keyCode,
                        event
                    )

        if (event.repeatCount == 0) {
            session.keyEvent(
                gameKey,
                true,
                KEY_VALUE_UNUSED
            )

            return true
        }

        return super.onKeyDown(
            keyCode,
            event
        )
    }

    override fun onKeyUp(
        keyCode: Int,
        event: KeyEvent
    ): Boolean {
        if (
            panelNav() != null &&
            (
                isPanelKey(keyCode) ||
                    consumeIfGamepad(event)
            )
        ) {
            return true
        }

        val gameKey =
            keyMap[keyCode]
                ?: return consumeIfGamepad(event) ||
                    super.onKeyUp(
                        keyCode,
                        event
                    )

        session.keyEvent(
            gameKey,
            false,
            KEY_VALUE_UNUSED
        )

        return true
    }

    private fun consumeIfGamepad(
        event: KeyEvent
    ): Boolean =
        event.keyCode != KeyEvent.KEYCODE_BACK &&
            (
                event.source and
                    InputDevice.SOURCE_GAMEPAD ==
                    InputDevice.SOURCE_GAMEPAD ||
                    event.source and
                    InputDevice.SOURCE_JOYSTICK ==
                    InputDevice.SOURCE_JOYSTICK
            )

    override fun onGenericMotionEvent(
        event: MotionEvent
    ): Boolean =
        onGenericMotion(event)

    private fun onGenericMotion(
        event: MotionEvent
    ): Boolean {
        panelNav()?.let { nav ->
            panelHat(
                nav,
                event
            )

            return true
        }

        val hatHandled =
            isNonDpadSource(event) &&
                handleHat(event)

        if (
            event.source and
                InputDevice.SOURCE_JOYSTICK !=
                InputDevice.SOURCE_JOYSTICK
        ) {
            return hatHandled ||
                super.onGenericMotionEvent(
                    event
                )
        }

        emitAxisPair(
            event.getAxisValue(
                MotionEvent.AXIS_X
            ),
            negKey = KC_LTHUMB_LEFT,
            posKey = KC_LTHUMB_RIGHT,
            invert = false
        )

        emitAxisPair(
            event.getAxisValue(
                MotionEvent.AXIS_Y
            ),
            negKey = KC_LTHUMB_UP,
            posKey = KC_LTHUMB_DOWN,
            invert = true
        )

        emitAxisPair(
            event.getAxisValue(
                MotionEvent.AXIS_Z
            ),
            negKey = KC_RTHUMB_LEFT,
            posKey = KC_RTHUMB_RIGHT,
            invert = false
        )

        emitAxisPair(
            event.getAxisValue(
                MotionEvent.AXIS_RZ
            ),
            negKey = KC_RTHUMB_UP,
            posKey = KC_RTHUMB_DOWN,
            invert = true
        )

        lTriggerDown =
            emitTrigger(
                maxOf(
                    event.getAxisValue(
                        MotionEvent.AXIS_LTRIGGER
                    ),
                    event.getAxisValue(
                        MotionEvent.AXIS_BRAKE
                    )
                ),
                KC_TRIGGER_L,
                lTriggerDown
            )

        rTriggerDown =
            emitTrigger(
                maxOf(
                    event.getAxisValue(
                        MotionEvent.AXIS_RTRIGGER
                    ),
                    event.getAxisValue(
                        MotionEvent.AXIS_GAS
                    )
                ),
                KC_TRIGGER_R,
                rTriggerDown
            )

        return true
    }

    private fun emitTrigger(
        value: Float,
        gameKey: Int,
        wasDown: Boolean
    ): Boolean {
        val down =
            value > 0.5f

        if (down != wasDown) {
            session.keyEvent(
                gameKey,
                down,
                KEY_VALUE_UNUSED
            )
        }

        return down
    }

    private fun emitAxisPair(
        axis: Float,
        negKey: Int,
        posKey: Int,
        invert: Boolean
    ) {
        val raw =
            if (invert) {
                -axis
            } else {
                axis
            }

        val v =
            if (
                abs(raw) <
                    AXIS_DEADZONE
            ) {
                0f
            } else {
                raw
            }

        when {
            v < 0f -> {
                emitAxis(
                    posKey,
                    false,
                    0
                )

                emitAxis(
                    negKey,
                    true,
                    (v * 32768f).toInt()
                )
            }

            v > 0f -> {
                emitAxis(
                    negKey,
                    false,
                    0
                )

                emitAxis(
                    posKey,
                    true,
                    (v * 32767f).toInt()
                )
            }

            else -> {
                emitAxis(
                    negKey,
                    false,
                    0
                )

                emitAxis(
                    posKey,
                    false,
                    0
                )
            }
        }
    }

    private val axisPressed =
        BooleanArray(24)

    private val axisValue =
        IntArray(24) {
            Int.MIN_VALUE
        }

    private fun emitAxis(
        code: Int,
        pressed: Boolean,
        value: Int
    ) {
        if (
            axisPressed[code] == pressed &&
            axisValue[code] == value
        ) {
            return
        }

        axisPressed[code] = pressed
        axisValue[code] = value

        session.keyEvent(
            code,
            pressed,
            value
        )
    }

    private var hatLeft = false
    private var hatUp = false
    private var hatRight = false
    private var hatDown = false

    private fun handleHat(
        event: MotionEvent
    ): Boolean {
        val hx =
            event.getAxisValue(
                MotionEvent.AXIS_HAT_X
            )

        val hy =
            event.getAxisValue(
                MotionEvent.AXIS_HAT_Y
            )

        val left =
            hx < -0.5f

        val right =
            hx > 0.5f

        val up =
            hy < -0.5f

        val down =
            hy > 0.5f

        if (left != hatLeft) {
            session.keyEvent(
                KC_DPAD_LEFT,
                left,
                KEY_VALUE_UNUSED
            )

            hatLeft = left
        }

        if (right != hatRight) {
            session.keyEvent(
                KC_DPAD_RIGHT,
                right,
                KEY_VALUE_UNUSED
            )

            hatRight = right
        }

        if (up != hatUp) {
            session.keyEvent(
                KC_DPAD_UP,
                up,
                KEY_VALUE_UNUSED
            )

            hatUp = up
        }

        if (down != hatDown) {
            session.keyEvent(
                KC_DPAD_DOWN,
                down,
                KEY_VALUE_UNUSED
            )

            hatDown = down
        }

        return left ||
            right ||
            up ||
            down
    }

    private fun isNonDpadSource(
        event: MotionEvent
    ): Boolean =
        event.source and
            InputDevice.SOURCE_DPAD !=
            InputDevice.SOURCE_DPAD

    private class PanelNav(
        val count: Int,
        val activate: (Int) -> Unit,
        val cancel: () -> Unit,
    )

    private fun panelNav(): PanelNav? {
        discRequestState.value?.let { req ->
            val paths =
                req.discPaths ?: emptyArray()

            val discs =
                minOf(
                    req.discLabels?.size ?: 0,
                    paths.size
                )

            return PanelNav(
                discs + 1,

                { i ->
                    if (i < discs) {
                        chooseDisc(
                            paths[i]
                        )
                    } else {
                        cancelDisc()
                    }
                },

                {
                    cancelDisc()
                }
            )
        }

        messageBoxRequestState.value?.let { req ->
            return PanelNav(
                req.buttons?.size ?: 0,

                { i ->
                    answerMessageBox(
                        i
                    )
                },

                {
                    answerMessageBox(
                        req.activeButton
                    )
                }
            )
        }

        keyboardRequestState.value?.let {
            return PanelNav(
                2,

                { i ->
                    if (i == 0) {
                        acceptKeyboard(
                            keyboardTextState.value
                        )
                    } else {
                        cancelKeyboard()
                    }
                },

                {
                    cancelKeyboard()
                }
            )
        }

        if (menuOpenState.value) {
            return PanelNav(
                PAUSE_OPTION_COUNT,

                { i ->
                    when (i) {
                        PAUSE_OPTION_QUIT ->
                            finish()

                        PAUSE_OPTION_TOUCH_OVERLAY ->
                            toggleTouchOverlay()

                        else ->
                            closeMenuAndResume()
                    }
                },

                {
                    closeMenuAndResume()
                }
            )
        }

        return null
    }

    private fun toggleTouchOverlay() {
        val next =
            showTouchOverlay.value != true

        showTouchOverlay.value =
            next

        session.setShowTouchOverlay(
            next
        )

        lifecycleScope.launch(
            Dispatchers.IO
        ) {
            runCatching {
                val handle =
                    ConfigStore(
                        applicationContext
                    ).openLive()

                handle.putBool(
                    "HID",
                    "show_touch_overlay",
                    next
                )

                handle.closeFile()
            }.onFailure {
                Log.w(
                    TAG,
                    "persisting show_touch_overlay failed",
                    it
                )
            }
        }
    }

    private fun closeMenuAndResume() {
        menuOpenState.value = false

        if (session.booted) {
            session.resumeIfPaused()
        }
    }

    private fun isPanelKey(
        keyCode: Int
    ): Boolean =
        when (keyCode) {
            KeyEvent.KEYCODE_DPAD_UP,
            KeyEvent.KEYCODE_DPAD_LEFT,
            KeyEvent.KEYCODE_DPAD_DOWN,
            KeyEvent.KEYCODE_DPAD_RIGHT,
            KeyEvent.KEYCODE_DPAD_CENTER,
            KeyEvent.KEYCODE_ENTER,
            KeyEvent.KEYCODE_BUTTON_A,
            KeyEvent.KEYCODE_BUTTON_B -> true

            else -> false
        }

    private fun panelKeyDown(
        nav: PanelNav,
        keyCode: Int
    ): Boolean =
        when (keyCode) {
            KeyEvent.KEYCODE_DPAD_UP,
            KeyEvent.KEYCODE_DPAD_LEFT -> {
                movePanelSelection(
                    nav,
                    -1
                )

                true
            }

            KeyEvent.KEYCODE_DPAD_DOWN,
            KeyEvent.KEYCODE_DPAD_RIGHT -> {
                movePanelSelection(
                    nav,
                    1
                )

                true
            }

            KeyEvent.KEYCODE_DPAD_CENTER,
            KeyEvent.KEYCODE_ENTER,
            KeyEvent.KEYCODE_BUTTON_A -> {
                nav.activate(
                    panelSelectedState.intValue
                )

                true
            }

            KeyEvent.KEYCODE_BUTTON_B -> {
                nav.cancel()
                true
            }

            else -> false
        }

    private var panelNavPrev = false
    private var panelNavNext = false

    private fun panelHat(
        nav: PanelNav,
        event: MotionEvent
    ) {
        val y =
            event.getAxisValue(
                MotionEvent.AXIS_HAT_Y
            ) +
                event.getAxisValue(
                    MotionEvent.AXIS_Y
                )

        val x =
            event.getAxisValue(
                MotionEvent.AXIS_HAT_X
            ) +
                event.getAxisValue(
                    MotionEvent.AXIS_X
                )

        val prev =
            y < -0.5f ||
                x < -0.5f

        val next =
            y > 0.5f ||
                x > 0.5f

        if (prev != panelNavPrev) {
            panelNavPrev = prev

            if (prev) {
                movePanelSelection(
                    nav,
                    -1
                )
            }
        }

        if (next != panelNavNext) {
            panelNavNext = next

            if (next) {
                movePanelSelection(
                    nav,
                    1
                )
            }
        }
    }

    private fun movePanelSelection(
        nav: PanelNav,
        delta: Int
    ) {
        if (nav.count <= 0) return

        val next =
            panelSelectedState.intValue +
                delta

        panelSelectedState.intValue =
            (
                (next % nav.count) +
                    nav.count
                ) % nav.count
    }

    private fun answerMessageBox(
        button: Int
    ) {
        val req =
            messageBoxRequestState.value
                ?: return

        session.messageBoxSubmit(
            req.id,
            button
        )

        messageBoxRequestState.value =
            null
    }

    private fun chooseDisc(
        path: String
    ) {
        val req =
            discRequestState.value
                ?: return

        session.discSubmit(
            req.id,
            true,
            path
        )

        discRequestState.value =
            null
    }

    private fun cancelDisc() {
        val req =
            discRequestState.value
                ?: return

        session.discSubmit(
            req.id,
            false,
            ""
        )

        discRequestState.value =
            null
    }

    private fun acceptKeyboard(
        text: String
    ) {
        val req =
            keyboardRequestState.value
                ?: return

        session.keyboardSubmit(
            req.id,
            true,
            text
        )

        keyboardRequestState.value =
            null
    }

    private fun cancelKeyboard() {
        val req =
            keyboardRequestState.value
                ?: return

        session.keyboardSubmit(
            req.id,
            false,
            ""
        )

        keyboardRequestState.value =
            null
    }
}

@Composable
private fun rememberOverlayContrast(
    brightness: StateFlow<Float>
): State<Float> {
    val anim =
        remember {
            Animatable(0f)
        }

    LaunchedEffect(brightness) {
        brightness.collect { b ->
            val target =
                (
                    (b - 0.5f) / 0.4f
                )
                    .coerceIn(
                        0f,
                        1f
                    )
                    .let {
                        (
                            it * 8f
                        )
                            .roundToInt() / 8f
                    }

            if (
                target !=
                    anim.targetValue
            ) {
                launch {
                    anim.animateTo(
                        target,
                        tween(400)
                    )
                }
            }
        }
    }

    return anim.asState()
}