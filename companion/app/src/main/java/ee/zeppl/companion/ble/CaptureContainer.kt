package ee.zeppl.companion.ble

import kotlinx.serialization.Serializable
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json

/**
 * The guided-capture dump (docs/multi-vrod-adaptive-layer.md §4): the raw bus
 * frames the cluster streamed (0x50, decoded by [RawFrameCodec]) plus a
 * synchronized event track of what the guided procedure told the rider to do and
 * when, plus bike metadata. This is what a rider of an unknown V-Rod submits so
 * we can build a decode profile for it.
 *
 * Privacy (design §4 return path): metadata carries NO VIN, NO GPS, and NO
 * wall-clock ride timestamps — only the relative device `tMs`.
 */
@Serializable
data class CaptureMetadata(
    val model: String? = null,     // e.g. "VRSCF"
    val yearRange: String? = null, // e.g. "2009" or "2004-2013"
    val trim: String? = null,
    val abs: Boolean? = null,
    val hfsm: Boolean? = null,
    val odometerKm: Int? = null,
    val ambientTempC: Int? = null,
    val fwVersion: String? = null,
)

@Serializable
data class CaptureFrame(val tMs: Long, val hex: String)

@Serializable
data class CaptureEvent(val tMs: Long, val stepId: String, val label: String, val action: String)

@Serializable
data class CaptureContainer(
    val version: Int = 1,
    val metadata: CaptureMetadata,
    val frames: List<CaptureFrame>,
    val events: List<CaptureEvent>,
)

private fun ByteArray.toHex(): String = joinToString("") { "%02X".format(it.toInt() and 0xFF) }

/**
 * Accumulates a capture in memory. Bounded by [maxFrames] — a hard cap so a long
 * session can't exhaust phone storage (design guardrail); frames past it are
 * dropped and counted, never silently lost.
 */
class CaptureSession(
    private val metadata: CaptureMetadata,
    private val maxFrames: Int = 50_000,
) {
    private val frames = ArrayList<CaptureFrame>()
    private val events = ArrayList<CaptureEvent>()

    var droppedFrames = 0
        private set

    val frameCount: Int get() = frames.size
    val eventCount: Int get() = events.size

    /** Record one decoded raw frame; drops (and counts) past the cap. */
    fun addFrame(f: RawFrameCodec.RawFrame) {
        if (frames.size >= maxFrames) {
            droppedFrames++
            return
        }
        frames.add(CaptureFrame(f.tMs, f.bytes.toHex()))
    }

    /** Label a moment in the event track (what the procedure told the rider). */
    fun addEvent(tMs: Long, stepId: String, label: String, action: String) {
        events.add(CaptureEvent(tMs, stepId, label, action))
    }

    fun toContainer(): CaptureContainer =
        CaptureContainer(metadata = metadata, frames = frames.toList(), events = events.toList())

    fun toJson(): String = JSON.encodeToString(toContainer())

    companion object {
        val JSON = Json { prettyPrint = false }
    }
}
