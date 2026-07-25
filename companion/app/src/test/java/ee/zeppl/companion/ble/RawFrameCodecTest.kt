package ee.zeppl.companion.ble

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

/**
 * The raw-sniff 0x50 frame layout, mirroring the firmware `raw_sniff_encode`
 * (`firmware/main/connectivity/phone/raw_sniff_codec.c`) and the fixture in
 * `test_apps/host/tests/test_raw_sniff_codec.c` (test_encode_layout). Touch one
 * side, touch both.
 */
class RawFrameCodecTest {

    @Test fun `decode matches the C test_encode_layout fixture`() {
        // 0x50, len=11, t_ms=0x01020304 (LE), then a real RPM frame incl. CRC.
        val frame = byteArrayOf(
            0x50, 0x0B, 0x00,
            0x04, 0x03, 0x02, 0x01,
            0x28, 0x1B, 0x10, 0x02, 0x13, 0xC4.toByte(), 0xF4.toByte(),
        )
        val r = RawFrameCodec.decode(frame)!!
        assertEquals(0x01020304L, r.tMs)
        assertArrayEquals(
            byteArrayOf(0x28, 0x1B, 0x10, 0x02, 0x13, 0xC4.toByte(), 0xF4.toByte()),
            r.bytes,
        )
    }

    @Test fun `high timestamp bit stays unsigned`() {
        // t_ms = 0xF0000001 must not sign-extend into a negative Long.
        val frame = byteArrayOf(0x50, 0x05, 0x00, 0x01, 0x00, 0x00, 0xF0.toByte(), 0x11)
        val r = RawFrameCodec.decode(frame)!!
        assertEquals(0xF0000001L, r.tMs)
        assertArrayEquals(byteArrayOf(0x11), r.bytes)
    }

    @Test fun `rejects wrong type`() {
        assertNull(RawFrameCodec.decode(byteArrayOf(0x40, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11)))
    }

    @Test fun `rejects short and truncated frames`() {
        assertNull(RawFrameCodec.decode(byteArrayOf(0x50, 0x0B, 0x00)))         // header only
        assertNull(RawFrameCodec.decode(byteArrayOf(0x50, 0x03, 0x00, 0, 0, 0)))  // len < 4 (no timestamp)
        // len says 11 but only 7 bytes of payload are present
        assertNull(RawFrameCodec.decode(byteArrayOf(0x50, 0x0B, 0x00, 0x04, 0x03, 0x02, 0x01, 0x28)))
    }

    @Test fun `equals compares timestamp and bytes by value`() {
        val a = RawFrameCodec.RawFrame(5L, byteArrayOf(1, 2, 3))
        val b = RawFrameCodec.RawFrame(5L, byteArrayOf(1, 2, 3))
        val c = RawFrameCodec.RawFrame(5L, byteArrayOf(1, 2, 4))
        assertEquals(a, b)
        assertEquals(a.hashCode(), b.hashCode())
        assertNull(if (a == c) "equal" else null)
    }
}
