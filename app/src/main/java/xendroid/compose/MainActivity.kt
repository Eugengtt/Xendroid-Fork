package xendroid.compose

import android.content.Intent
import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.runtime.getValue
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.launch
import xendroid.compose.core.EmulatorRuntime
import xendroid.compose.core.FrontendLaunch
import xendroid.compose.ui.library.ACTION_LAUNCH_GAME
import xendroid.compose.ui.library.EXTRA_GAME_URI
import xendroid.compose.core.SessionLogs
import xendroid.compose.ui.AppNavHost
import xendroid.compose.ui.theme.xendroidTheme
import xendroid.compose.settings.ConfigStore
import xendroid.compose.settings.seedTouchOverlayDefault
import xendroid.compose.updater.LatestVersionDialog
import xendroid.compose.updater.UpdateDialog
import xendroid.compose.updater.UpdateResult
import xendroid.compose.updater.checkForUpdates
import xendroid.compose.updater.shouldCheckForUpdates
import xendroid.compose.updater.saveLastCheck
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.concurrent.thread


class MainActivity : ComponentActivity() {

    companion object {
        // One rotation per main-process lifetime = the app-session boundary.
        private val sessionRotated = AtomicBoolean(false)
    }

    private var updateResult by mutableStateOf<UpdateResult?>(null)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Same intent shape as the in-app launch, which routes through the
        // manifest filter into the host's own task. Finishing here instead
        // collapses that task and the emulator is paused before it draws.
        val frontendGame = FrontendLaunch.resolveGamePath(this, intent)
        if (frontendGame != null) {
            startActivity(
                Intent(ACTION_LAUNCH_GAME).apply {
                    setPackage(packageName)
                    putExtra(EXTRA_GAME_URI, frontendGame)
                }
            )
        }

        if (!sessionRotated.getAndSet(true)) {
            val appContext = applicationContext

            thread(name = "SessionLogs") {
                runCatching { SessionLogs.startAppSession(appContext) }

                // Pre-warm so settings doesn't pay the delay-load System.loadLibrary.
                runCatching { EmulatorRuntime.ensureLoaded() }
                // Needs the native config, so it follows ensureLoaded on this same thread.
                runCatching { seedTouchOverlayDefault(appContext, ConfigStore(appContext)) }
            }
        }

        val container = AppContainer(applicationContext)

        enableEdgeToEdge()

       setContent {
            xendroidTheme {

                AppNavHost(container)

                LaunchedEffect(Unit) {
                    if (frontendGame != null) return@LaunchedEffect
                    // A debug build's -debug versionName never matches a release tag, so the
                    // check always reports an update - to an APK whose .debug-suffixed package
                    // could not replace this install anyway.
                    if (BuildConfig.DEBUG) return@LaunchedEffect
                    if (!shouldCheckForUpdates(applicationContext)) {
                        Log.d("Updater", "Skipping update check (less than 5 minutes)")
                        return@LaunchedEffect
                    }

                    try {
                        val result = checkForUpdates()

                        updateResult = result

                        // Export check result only if github replied with a valid response
                        saveLastCheck(applicationContext)
                    } catch (e: Exception) {
                        Log.e(
                            "Updater",
                            "Failed to check updates",
                            e
                        )
                    }
                }

                when (val result = updateResult) {
                    is UpdateResult.Available -> {
                        UpdateDialog(
                            release = result.release,
                            onDismiss = { updateResult = null }
                        )
                    }

                    is UpdateResult.Latest -> {
                        LatestVersionDialog(
                            commitHash = result.commitHash,
                            onDismiss = { updateResult = null }
                        )
                    }

                    is UpdateResult.Cooldown, null -> {}
                }
            }
        }
    }
}