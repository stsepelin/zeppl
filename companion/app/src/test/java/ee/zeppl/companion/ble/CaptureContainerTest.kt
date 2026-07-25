package ee.zeppl.companion.ble

import kotlinx.serialization.decodeFromString
import org.junit.Assert.assertEquals
import org.junit.Test

/**
 * The guided-capture container model: accumulation, the hard-cap guardrail, the
 * labelled event track, and JSON round-tripping. Pure JVM (kotlinx.serialization).
 */
class CaptureContainerTest {

    private fun frame(tMs: Long, vararg bytes: Int) =
        RawFrameCodec.RawFrame(tMs, ByteArray(bytes.size) { bytes[it].toByte() })

    @Test fun `accumulates frames as uppercase hex incl high bytes`() {
        val s = CaptureSession(CaptureMetadata(model = "VRSCF"))
        s.addFrame(frame(100, 0x28, 0x1B, 0x10))
        s.addFrame(frame(150, 0xA8, 0x49, 0xD5)) // high bytes must not sign-extend
        assertEquals(2, s.frameCount)
        val c = s.toContainer()
        assertEquals(100L, c.frames[0].tMs)
        assertEquals("281B10", c.frames[0].hex)
        assertEquals("A849D5", c.frames[1].hex)
    }

    @Test fun `hard cap drops and counts overflow frames`() {
        val s = CaptureSession(CaptureMetadata(), maxFrames = 2)
        repeat(5) { s.addFrame(frame(it.toLong(), 0x01)) }
        assertEquals(2, s.frameCount)
        assertEquals(3, s.droppedFrames)
    }

    @Test fun `event track records labelled steps`() {
        val s = CaptureSession(CaptureMetadata())
        s.addEvent(10, "left_turn", "Signal left", "start")
        s.addEvent(20, "left_turn", "Signal left", "confirm")
        assertEquals(2, s.eventCount)
        val c = s.toContainer()
        assertEquals("left_turn", c.events[0].stepId)
        assertEquals("start", c.events[0].action)
    }

    @Test fun `json round-trips the whole container`() {
        val s = CaptureSession(CaptureMetadata(model = "VRSCF", yearRange = "2009", abs = false))
        s.addFrame(frame(100, 0x28, 0x1B, 0x10, 0x02, 0x00, 0x00, 0xD5))
        s.addEvent(50, "idle", "Cold idle", "start")

        val back = CaptureSession.JSON.decodeFromString<CaptureContainer>(s.toJson())
        assertEquals(s.toContainer(), back)
        assertEquals(1, back.version)
        assertEquals("VRSCF", back.metadata.model)
        assertEquals(false, back.metadata.abs)
        assertEquals("281B10020000D5", back.frames[0].hex)
    }
}
