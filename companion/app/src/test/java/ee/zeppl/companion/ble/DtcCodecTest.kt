package ee.zeppl.companion.ble

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

/**
 * The DTC request/result byte layout, mirroring the firmware's
 * `dtc_result_encode` / `dtc_format` (`firmware/main/j1850/dtc.c`) and the
 * fixture in `test_apps/host/tests/test_dtc.c` (test_dtc_result_encode).
 * Touch one side, touch both.
 */
class DtcCodecTest {

    @Test fun `request encodes the 1-byte sub-command`() {
        assertArrayEquals(byteArrayOf(0x09, 0x01, 0x00, 0x00), Protocol.encodeDtcRequest(false))
        assertArrayEquals(byteArrayOf(0x09, 0x01, 0x00, 0x01), Protocol.encodeDtcRequest(true))
    }

    @Test fun `format matches the firmware dtc_format`() {
        assertEquals("U1255", DtcCodec.format(0xD2, 0x55))
        assertEquals("P0107", DtcCodec.format(0x01, 0x07))
        assertEquals("C1014", DtcCodec.format(0x50, 0x14))
        assertEquals("B1121", DtcCodec.format(0x91, 0x21))
    }

    @Test fun `decode matches the C test_dtc_result_encode fixture`() {
        // 0x41, len=9, op=read, status=ok, count=2, ECM U1255, TSM P0107.
        val frame = byteArrayOf(
            0x41, 0x09, 0x00, 0x00, 0x00, 0x02,
            0x10.toByte(), 0xD2.toByte(), 0x55,
            0x40, 0x01, 0x07,
        )
        val r = DtcCodec.decode(frame)!!
        assertEquals(DtcCodec.OP_READ, r.op)
        assertEquals(DtcCodec.STATUS_OK, r.status)
        assertEquals(2, r.codes.size)
        assertEquals("ECM", r.codes[0].moduleName)
        assertEquals("U1255", r.codes[0].text)
        assertEquals("TSM/TSSM", r.codes[1].moduleName)
        assertEquals("P0107", r.codes[1].text)
    }

    @Test fun `decode a clean read and a clear ack`() {
        val clean = DtcCodec.decode(byteArrayOf(0x41, 0x03, 0x00, 0x00, 0x00, 0x00))!!
        assertEquals(0, clean.codes.size)
        val clr = DtcCodec.decode(byteArrayOf(0x41, 0x03, 0x00, 0x01, 0x00, 0x00))!!
        assertEquals(DtcCodec.OP_CLEAR, clr.op)
    }

    @Test fun `malformed frames decode to null`() {
        assertNull(DtcCodec.decode(byteArrayOf(0x40, 0x03, 0x00, 0x00, 0x00, 0x00)))  // wrong type
        assertNull(DtcCodec.decode(byteArrayOf(0x41, 0x03, 0x00)))                    // truncated
        // count says 1 but the triplet is missing.
        assertNull(DtcCodec.decode(byteArrayOf(0x41, 0x06, 0x00, 0x00, 0x00, 0x01)))
    }
}
