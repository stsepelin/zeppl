package ee.zeppl.companion.ble

import android.os.SystemClock
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

/**
 * Owns the active guided-capture session and receives raw frames off the notify
 * path (0x50). Observable singleton like [DtcState]; the capture UI reads
 * `active` / `frameCount` and re-renders. Frames are recorded only while a
 * capture is active, so the high-rate 0x50 stream is ignored otherwise.
 */
object CaptureState {
    var active: Boolean by mutableStateOf(false)
        private set
    var frameCount: Int by mutableStateOf(0)
        private set
    var droppedFrames: Int by mutableStateOf(0)
        private set

    private var session: CaptureSession? = null

    fun start(metadata: CaptureMetadata) {
        session = CaptureSession(metadata)
        frameCount = 0
        droppedFrames = 0
        active = true
    }

    /** A decoded 0x50 frame from the notify path; ignored unless capturing. */
    fun observe(frame: RawFrameCodec.RawFrame) {
        val s = session ?: return
        s.addFrame(frame)
        frameCount = s.frameCount
        droppedFrames = s.droppedFrames
    }

    /** Label a procedure step in the event track (start / confirm / skip). */
    fun label(stepId: String, label: String, action: String) {
        session?.addEvent(SystemClock.uptimeMillis(), stepId, label, action)
    }

    /** Stop and hand back the finished dump JSON (null if nothing was captured). */
    fun stop(): String? {
        val json = session?.toJson()
        active = false
        session = null
        return json
    }
}
