package ee.zeppl.companion.ble

/**
 * Decoder for the cluster's DTC result frame (0x41), the answer to a
 * [Protocol.encodeDtcRequest]. Mirrors the firmware `dtc_result_encode` /
 * `dtc_format` in `firmware/main/j1850/dtc.c` (host-tested); `DtcCodecTest`
 * cross-checks the exact frame the firmware emits. Touch one, touch both.
 *
 * Frame: [0x41][u16 LE len][op][status][count][{module,hi,lo} x count].
 */
object DtcCodec {
    const val TYPE: Byte = 0x41

    const val OP_READ = 0
    const val OP_CLEAR = 1
    const val STATUS_OK = 0
    const val STATUS_NO_REPLY = 1  // a module never answered the request

    // Module addresses mirror dtc.h DTC_MODULE_*.
    fun moduleName(m: Int): String = when (m) {
        0x10 -> "ECM"
        0x40 -> "TSM/TSSM"
        0x60 -> "Other"
        else -> "0x%02X".format(m)
    }

    /** Raw SAE J2012 code pair -> its text, e.g. (0xD2, 0x55) -> "U1255". */
    fun format(hi: Int, lo: Int): String {
        val letter = "PCBU"[(hi shr 6) and 0x3]
        return "%c%X%X%X%X".format(
            letter, (hi shr 4) and 0x3, hi and 0x0F, (lo shr 4) and 0x0F, lo and 0x0F,
        )
    }

    data class Code(val module: Int, val moduleName: String, val text: String)
    data class Result(val op: Int, val status: Int, val codes: List<Code>)

    /** Decode a 0x41 frame; null if it isn't a well-formed DTC result. */
    fun decode(bytes: ByteArray): Result? {
        if (bytes.size < 6 || bytes[0] != TYPE) return null
        val len = (bytes[1].toInt() and 0xFF) or ((bytes[2].toInt() and 0xFF) shl 8)
        if (len < 3 || bytes.size < 3 + len) return null
        val op = bytes[3].toInt() and 0xFF
        val status = bytes[4].toInt() and 0xFF
        val count = bytes[5].toInt() and 0xFF
        if (bytes.size < 6 + count * 3) return null
        val codes = ArrayList<Code>(count)
        for (i in 0 until count) {
            val b = 6 + i * 3
            val m = bytes[b].toInt() and 0xFF
            codes.add(Code(m, moduleName(m), format(bytes[b + 1].toInt() and 0xFF, bytes[b + 2].toInt() and 0xFF)))
        }
        return Result(op, status, codes)
    }
}
