package xendroid.compose.core

import android.graphics.Bitmap
import android.os.Handler
import android.os.HandlerThread
import android.view.PixelCopy
import android.view.SurfaceView
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

/**
 * Periodically samples the emulator SurfaceView with PixelCopy, averages its luminance, and
 * publishes it as [brightness] in [0,1]. Entirely off the render path: a low-res copy a few
 * times a second on its own thread. The touch overlay uses it to stay visible on bright scenes.
 */
class ScreenBrightnessSampler(private val intervalMs: Long = 350L) {
    private val _brightness = MutableStateFlow(0f)
    val brightness: StateFlow<Float> = _brightness

    private val sample = 24
    private val bitmap = Bitmap.createBitmap(sample, sample, Bitmap.Config.ARGB_8888)
    private val pixels = IntArray(sample * sample)

    private var thread: HandlerThread? = null
    private var handler: Handler? = null
    private var view: SurfaceView? = null
    @Volatile private var running = false

    fun start(surfaceView: SurfaceView) {
        if (running) return
        view = surfaceView
        running = true
        val t = HandlerThread("overlay-brightness").also { it.start() }
        thread = t
        handler = Handler(t.looper).also { it.post(::sampleOnce) }
    }

    fun stop() {
        running = false
        handler?.removeCallbacksAndMessages(null)
        thread?.quitSafely()
        thread = null; handler = null; view = null
    }

    private fun scheduleNext() {
        if (running) handler?.postDelayed(::sampleOnce, intervalMs)
    }

    private fun sampleOnce() {
        val sv = view ?: return
        val h = handler ?: return
        if (!running || sv.width == 0 || sv.height == 0 || sv.holder.surface?.isValid != true) {
            scheduleNext(); return
        }
        try {
            PixelCopy.request(sv, bitmap, { result ->
                if (result == PixelCopy.SUCCESS) {
                    bitmap.getPixels(pixels, 0, sample, 0, 0, sample, sample)
                    var sum = 0.0
                    for (p in pixels) {
                        val r = (p shr 16) and 0xFF
                        val g = (p shr 8) and 0xFF
                        val b = p and 0xFF
                        sum += 0.299 * r + 0.587 * g + 0.114 * b
                    }
                    val lum = (sum / pixels.size / 255.0).toFloat()
                    // EMA smoothing so a single flashing frame doesn't flicker the overlay.
                    _brightness.value = _brightness.value * 0.6f + lum * 0.4f
                }
                scheduleNext()
            }, h)
        } catch (e: IllegalArgumentException) {
            // Surface went away between the validity check and the request.
            scheduleNext()
        }
    }
}
