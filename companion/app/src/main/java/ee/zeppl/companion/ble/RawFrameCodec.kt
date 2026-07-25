package ee.zeppl.companion.ble

/**
 * Decoder for the cluster's raw-sniff frame (0x50) — every J1850 bus frame the
 * cluster sees, streamed for the multi-V-Rod adaptive layer's guided capture.
 * Mirrors the firmware `raw_sniff_encode`
 * (`firmware/main/connectivity/phone/raw_sniff_codec.c`, host-tested);
 * `RawFrameCodecTest` cross-checks the exact bytes the firmware emits. Touch one,
 * touch both.
 *
 * Frame: [0x50][u16 LE payload_len][u32 LE t_ms][frame bytes incl. CRC].
 */
object RawFrameCodec {
    const val TYPE: Byte = 0x50

    /** One captured bus frame: the device timestamp (ms) and the verbatim bytes. */
    class RawFrame(val tMs: Long, val bytes: ByteArray) {
        override fun equals(other: Any?): Boolean =
            other is RawFrame && tMs == other.tMs && bytes.contentEquals(other.bytes)

        override fun hashCode(): Int = 31 * tMs.hashCode() + bytes.contentHashCode()
    }

    /** Decode a 0x50 frame; null if it isn't a well-formed raw frame. */
    fun decode(bytes: ByteArray): RawFrame? {
        if (bytes.size < 7 || bytes[0] != TYPE) return null
        val len = (bytes[1].toInt() and 0xFF) or ((bytes[2].toInt() and 0xFF) shl 8)
        // payload is >= the 4-byte timestamp; the frame bytes follow it.
        if (len < 4 || bytes.size < 3 + len) return null
        val tMs = (bytes[3].toLong() and 0xFF) or
            ((bytes[4].toLong() and 0xFF) shl 8) or
            ((bytes[5].toLong() and 0xFF) shl 16) or
            ((bytes[6].toLong() and 0xFF) shl 24)
        val frame = bytes.copyOfRange(7, 3 + len)
        return RawFrame(tMs, frame)
    }
}
