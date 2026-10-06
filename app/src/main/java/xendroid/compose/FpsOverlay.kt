package xendroid.compose

import android.app.ActivityManager
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.Process
import android.os.SystemClock
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTransformGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import java.io.File
import java.util.Locale
import kotlin.math.roundToInt
import xendroid.compose.core.EmulatorSession

private data class GpuCounter(
    val busy: Long,
    val total: Long,
)

// Compiled once. The counter parser used to build this Regex on every poll, which paid the
// pattern-compile cost 4x/second while the overlay was up.
private val GPU_COUNTER_SPLIT = Regex("\\s+")

private data class GpuSource(
    val directPercentFile: File? = null,
    val counterFile: File? = null,
)

private fun findGpuSource(): GpuSource? {
    return runCatching {
        val kgslPercent = File("/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage")
        if (kgslPercent.isFile) {
            return GpuSource(directPercentFile = kgslPercent)
        }

        val kgslDevfreqLoad = File("/sys/class/kgsl/kgsl-3d0/devfreq/gpu_load")
        if (kgslDevfreqLoad.isFile) {
            return GpuSource(directPercentFile = kgslDevfreqLoad)
        }

        val devfreq = File("/sys/class/devfreq")
        devfreq.listFiles()?.forEach { node ->
            val name = node.name.lowercase(Locale.US)
            if (name.contains("gpu") || name.contains("kgsl") || name.contains("3d")) {
                val load = File(node, "load")
                if (load.isFile) {
                    return GpuSource(directPercentFile = load)
                }
            }
        }

        val kgslBusy = File("/sys/class/kgsl/kgsl-3d0/gpubusy")
        if (kgslBusy.isFile) {
            return GpuSource(counterFile = kgslBusy)
        }

        null
    }.getOrNull()
}

private fun readDirectGpuPercent(file: File): Int? {
    return runCatching {
        file.readText()
            .trim()
            .removeSuffix("%")
            .toFloatOrNull()
            ?.coerceIn(0f, 100f)
            ?.roundToInt()
    }.getOrNull()
}

private fun readGpuCounter(file: File): GpuCounter? {
    return runCatching {
        val values = file.readText().trim().split(GPU_COUNTER_SPLIT)
        if (values.size < 2) return null
        val busy = values[0].toLongOrNull() ?: return null
        val total = values[1].toLongOrNull() ?: return null
        if (busy < 0L || total <= 0L) return null
        GpuCounter(busy = busy, total = total)
    }.getOrNull()
}

private fun readRamUsage(context: Context): Pair<Long, Long> {
    val manager = context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
    val info = ActivityManager.MemoryInfo()
    manager.getMemoryInfo(info)
    return (info.totalMem - info.availMem) to info.totalMem
}

private fun readBatteryTemperature(context: Context): Float {
    val intent = context.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
    val temp = intent?.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0) ?: 0
    return temp / 10.0f
}

/**
 * RAM and battery temperature are slow-moving values, but each sample was costing a binder
 * round-trip on every poll (plus a broadcast Intent lookup for the battery). Refreshing at
 * 1 Hz instead of the overlay's poll rate drops 3 of every 4 of those transactions while
 * the displayed numbers stay identical to the eye - neither moves on a sub-second scale.
 */
private class SlowStatsCache {
    private var lastReadMs = 0L
    private var ramUsed = 0L
    private var ramTotal = 0L
    private var batTemp = 0f

    fun read(context: Context): Triple<Long, Long, Float> {
        val now = SystemClock.elapsedRealtime()
        if (now - lastReadMs >= 1_000L) {
            lastReadMs = now
            val (used, total) = readRamUsage(context)
            ramUsed = used
            ramTotal = total
            batTemp = readBatteryTemperature(context)
        }
        return Triple(ramUsed, ramTotal, batTemp)
    }
}

private fun parseZoneTemp(raw: Float?): Float? {
    if (raw == null || raw <= 0f) return null
    val temp = if (raw > 1000f) raw / 1000f else raw
    return if (temp in 10f..115f) temp else null
}

/**
 * Resolves the CPU/SoC thermal zone once and reuses it.
 *
 * The zone layout is fixed for the lifetime of the process, so re-listing `/sys/class/thermal`
 * and reading every zone's `type` file on each poll was pure waste: dozens of sysfs reads plus
 * String/lowercase churn per second, 4x a second, for a value that never changes.
 *
 * Only the zone's `temp` file is re-read per tick. If that file becomes unreadable the zone
 * vanished (permissions/namespace) and discovery is retried, but at most every 5s so a broken
 * kernel can't turn into a per-tick directory walk again.
 */
private class ThermalZoneCache {
    @Volatile private var tempFile: File? = null
    @Volatile private var attempted = false
    @Volatile private var lastAttemptMs = 0L

    private fun discover(): File? = runCatching {
        File("/sys/class/thermal").listFiles()?.forEach { zone ->
            if (!zone.name.startsWith("thermal_zone")) return@forEach
            val typeFile = File(zone, "type")
            val candidate = File(zone, "temp")
            if (!typeFile.isFile || !candidate.isFile) return@forEach
            val type = typeFile.readText().lowercase(Locale.US)
            if (type.contains("cpu") || type.contains("soc") ||
                type.contains("tsens") || type.contains("mtk")
            ) {
                return candidate
            }
        }
        null
    }.getOrNull()

    fun read(): Float? {
        val cached = tempFile
        if (cached != null) {
            val raw = runCatching { cached.readText().trim().toFloatOrNull() }.getOrNull()
            // A readable-but-odd value keeps the cache: parseZoneTemp just reports null for it.
            if (raw != null) return parseZoneTemp(raw)
            tempFile = null // zone gone -> rediscover below
        }
        val now = SystemClock.elapsedRealtime()
        if (attempted && now - lastAttemptMs < 5_000L) return null
        lastAttemptMs = now
        attempted = true
        return discover()?.also { tempFile = it }
            ?.let { runCatching { it.readText().trim().toFloatOrNull() }.getOrNull() }
            ?.let(::parseZoneTemp)
    }
}

private val cpuZoneCache = ThermalZoneCache()

private fun readCpuSocTemperature(): Float? = cpuZoneCache.read()

@Composable
fun FpsOverlay(
    session: EmulatorSession,
    visible: Boolean,
    modifier: Modifier = Modifier,
    pollHz: Int = 4,
    baseFontSizeSp: Float = 9f
) {
    if (!visible) return

    val context = LocalContext.current
    val prefs = remember { context.getSharedPreferences("fps_overlay", Context.MODE_PRIVATE) }

    var offset by remember {
        mutableStateOf(Offset(prefs.getFloat("x", 0f), prefs.getFloat("y", 0f)))
    }
    var scale by remember {
        mutableStateOf(prefs.getFloat("scale", 1.0f))
    }

    var containerSize by remember { mutableStateOf(IntSize.Zero) }
    val currentContainerSize by rememberUpdatedState(containerSize)

    var fps by remember { mutableStateOf(0.0) }
    var frameMs by remember { mutableStateOf(0.0) }
    var cpu by remember { mutableStateOf(0f) }
    var gpu by remember { mutableStateOf<Int?>(null) }
    var ramUsed by remember { mutableStateOf(0L) }
    var ramTotal by remember { mutableStateOf(0L) }
    var batTemp by remember { mutableStateOf(0f) }
    var socTemp by remember { mutableStateOf<Float?>(null) }

    LaunchedEffect(pollHz) {
        val periodMs = 1000L / pollHz.coerceIn(1, 10)
        val cpuCount = Runtime.getRuntime().availableProcessors().coerceAtLeast(1)

        var previousCpuMs = Process.getElapsedCpuTime()
        var previousWallNs = SystemClock.elapsedRealtimeNanos()
        var previousGpu: GpuCounter? = null
        var gpuSource: GpuSource? = null
        val slowStats = SlowStatsCache()

        while (true) {
            val currentFps = session.averageFps()
            val currentFrameMs = session.lastFrameTimeMs()

            val stats = withContext(Dispatchers.IO) {
                if (gpuSource == null) {
                    gpuSource = findGpuSource()
                }

                val newCpuMs = Process.getElapsedCpuTime()
                val newWallNs = SystemClock.elapsedRealtimeNanos()
                val cpuDeltaMs = newCpuMs - previousCpuMs
                val wallDeltaMs = (newWallNs - previousWallNs) / 1_000_000.0

                val cpuUsage = if (cpuDeltaMs >= 0L && wallDeltaMs > 0.0) {
                    (cpuDeltaMs.toDouble() / wallDeltaMs / cpuCount.toDouble() * 100.0)
                        .coerceIn(0.0, 100.0)
                        .toFloat()
                } else {
                    0f
                }

                previousCpuMs = newCpuMs
                previousWallNs = newWallNs

                var gpuUsage: Int? = null
                val source = gpuSource

                if (source?.directPercentFile != null) {
                    gpuUsage = readDirectGpuPercent(source.directPercentFile)
                } else if (source?.counterFile != null) {
                    val currentGpu = readGpuCounter(source.counterFile)
                    if (previousGpu != null && currentGpu != null) {
                        val busyDelta = currentGpu.busy - previousGpu!!.busy
                        val totalDelta = currentGpu.total - previousGpu!!.total
                        if (busyDelta >= 0L && totalDelta > 0L) {
                            gpuUsage = (busyDelta.toDouble() / totalDelta.toDouble() * 100.0)
                                .coerceIn(0.0, 100.0)
                                .roundToInt()
                        }
                    }
                    previousGpu = currentGpu
                }

                val sTemp = readCpuSocTemperature()
                val slow = slowStats.read(context)
                Triple(cpuUsage, gpuUsage, slow) to sTemp
            }

            fps = currentFps
            frameMs = currentFrameMs
            cpu = stats.first.first
            if (stats.first.second != null) gpu = stats.first.second
            ramUsed = stats.first.third.first
            ramTotal = stats.first.third.second
            batTemp = stats.first.third.third
            socTemp = stats.second

            delay(periodMs)
        }
    }

    Box(
        modifier = modifier
            .fillMaxSize()
            .onSizeChanged { containerSize = it }
    ) {
        val usedGb = ramUsed / 1_073_741_824.0
        val totalGb = ramTotal / 1_073_741_824.0

        Text(
            text = buildString {
                append(String.format(Locale.US, "FPS %.0f · %.1f ms\n", fps, frameMs))
                append(String.format(Locale.US, "CPU %.0f%%\n", cpu))
                append(gpu?.let { "GPU $it%\n" } ?: "GPU N/A\n")
                append(String.format(Locale.US, "RAM %.1f/%.1f GB\n", usedGb, totalGb))
                append(String.format(Locale.US, "BAT %.1f°C\n", batTemp))
                append(socTemp?.let { String.format(Locale.US, "SoC/CPU %.0f°C", it) } ?: "SoC/CPU N/A")
            },
            color = Color.White.copy(alpha = 0.85f),
            fontSize = (baseFontSizeSp * scale).sp,
            fontFamily = FontFamily.Monospace,
            modifier = Modifier
                .offset { IntOffset(offset.x.roundToInt(), offset.y.roundToInt()) }
                .background(Color.Black.copy(alpha = 0.45f), RoundedCornerShape(4.dp))
                .padding(horizontal = (4 * scale).dp, vertical = (3 * scale).dp)
                .pointerInput(Unit) {
                    detectTransformGestures { _, pan, zoom, _ ->
                        scale = (scale * zoom).coerceIn(0.5f, 2.5f)

                        val bounds = currentContainerSize
                        val maxX = maxOf(0f, bounds.width.toFloat() - size.width)
                        val maxY = maxOf(0f, bounds.height.toFloat() - size.height)

                        offset = Offset(
                            x = (offset.x + pan.x).coerceIn(0f, maxX),
                            y = (offset.y + pan.y).coerceIn(0f, maxY)
                        )

                        prefs.edit()
                            .putFloat("x", offset.x)
                            .putFloat("y", offset.y)
                            .putFloat("scale", scale)
                            .apply()
                    }
                }
        )
    }
}