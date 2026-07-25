package ee.zeppl.companion.ble

/**
 * Learning mode (docs/multi-vrod-adaptive-layer.md §3): from a labelled capture
 * dump, propose which frame bit tracks a switch the rider toggled — automating
 * the manual "toggle one input, diff the frames" method that mapped the turn
 * signals by hand. It **proposes**; a human confirms before anything becomes a
 * profile entry (nothing here is auto-promoted).
 */
object LearningCorrelator {

    /** A proposed flag mapping: a bit that toggled in lockstep with a step. */
    data class FlagProposal(
        val header: String, // 3-byte header as hex, e.g. "48DA40"
        val offset: Int, // byte index into the frame
        val mask: Int, // the bit
        val confidencePct: Int, // agreement over that header's frames, 0..100
    )

    private data class Window(val startMs: Long, val endMs: Long)

    // ON windows for a step: pair each start -> the next confirm, in order.
    private fun windowsFor(container: CaptureContainer, stepId: String): List<Window> {
        val out = ArrayList<Window>()
        var open: Long? = null
        for (e in container.events) {
            if (e.stepId != stepId) continue
            when (e.action) {
                "start" -> open = e.tMs
                "confirm" -> open?.let { out.add(Window(it, e.tMs)); open = null }
            }
        }
        return out
    }

    private fun hexToBytes(hex: String): IntArray =
        IntArray(hex.length / 2) { hex.substring(it * 2, it * 2 + 2).toInt(16) }

    private fun inAnyWindow(t: Long, windows: List<Window>): Boolean =
        windows.any { t >= it.startMs && t <= it.endMs }

    /**
     * Find the (header, offset, bit) whose value best matches the step's ON
     * windows — set inside a window, clear outside. Returns the best candidate at
     * or above [minConfidencePct], or null if none qualifies (or the step was
     * never windowed / no usable frames).
     */
    fun proposeFlag(
        container: CaptureContainer,
        stepId: String,
        minConfidencePct: Int = 90,
    ): FlagProposal? {
        val windows = windowsFor(container, stepId)
        if (windows.isEmpty()) return null

        val frames = container.frames.map { it.tMs to hexToBytes(it.hex) }
            .filter { it.second.size >= 4 } // header + at least one data byte + CRC
        if (frames.isEmpty()) return null

        var best: FlagProposal? = null
        val headers = frames.map { it.second.take(3) }.toSet()
        for (h in headers) {
            val hframes = frames.filter { it.second.take(3) == h }
            val maxLen = hframes.maxOf { it.second.size }
            for (offset in 3 until maxLen - 1) { // data bytes: exclude header + CRC
                for (bit in 0 until 8) {
                    val mask = 1 shl bit
                    var counted = 0
                    var agree = 0
                    for ((t, bytes) in hframes) {
                        if (offset > bytes.size - 2) continue // offset in CRC / past end
                        counted++
                        val set = (bytes[offset] and mask) != 0
                        if (set == inAnyWindow(t, windows)) agree++
                    }
                    if (counted == 0) continue
                    val conf = agree * 100 / counted
                    if (best == null || conf > best!!.confidencePct) {
                        best = FlagProposal(h.joinToString("") { "%02X".format(it) }, offset, mask, conf)
                    }
                }
            }
        }
        return best?.takeIf { it.confidencePct >= minConfidencePct }
    }

    /** A proposed value mapping: a scaled numeric field, `eng = raw*scale + bias`. */
    data class ValueProposal(
        val header: String,
        val offset: Int,
        val width: Int, // 1 or 2 (big-endian)
        val scale: Double,
        val bias: Double,
        val r2Pct: Int, // goodness of fit, 0..100
    )

    private data class Fit(val slope: Double, val intercept: Double, val r2: Double)

    private fun readBe(bytes: IntArray, offset: Int, width: Int): Int =
        if (width == 2) (bytes[offset] shl 8) or bytes[offset + 1] else bytes[offset]

    private fun linearFit(x: List<Double>, y: List<Double>): Fit? {
        val mx = x.average()
        val my = y.average()
        var sxx = 0.0
        var sxy = 0.0
        var syy = 0.0
        for (i in x.indices) {
            val dx = x[i] - mx
            val dy = y[i] - my
            sxx += dx * dx
            sxy += dx * dy
            syy += dy * dy
        }
        if (sxx == 0.0) return null // raw never varied -> no fit
        val slope = sxy / sxx
        val r2 = if (syy == 0.0) 0.0 else (sxy * sxy) / (sxx * syy)
        return Fit(slope, my - slope * mx, r2)
    }

    /**
     * Find the numeric field (header, offset, 1/2-byte width) that best fits a set
     * of engineering-unit reference points — GPS speed, a two-point temperature,
     * or the already-decoded RPM. Solves `eng = raw*scale + bias` by linear
     * regression (needs >= 2 reference points); returns the best fit at or above
     * [minR2Pct], or null. This is the "scaling needs two reference points" path —
     * it also covers monotonic-with-RPM when the reference is the decoded RPM.
     */
    fun proposeValue(
        container: CaptureContainer,
        reference: List<Pair<Long, Double>>,
        minR2Pct: Int = 95,
    ): ValueProposal? {
        if (reference.size < 2) return null
        val frames = container.frames.map { it.tMs to hexToBytes(it.hex) }.filter { it.second.size >= 4 }
        if (frames.isEmpty()) return null

        var best: ValueProposal? = null
        val headers = frames.map { it.second.take(3) }.toSet()
        for (h in headers) {
            val hframes = frames.filter { it.second.take(3) == h }
            val maxLen = hframes.maxOf { it.second.size }
            for (width in 1..2) {
                for (offset in 3..maxLen - 1 - width) { // width data bytes before the CRC
                    val xs = ArrayList<Double>()
                    val ys = ArrayList<Double>()
                    for ((tRef, vRef) in reference) {
                        val near = hframes
                            .filter { offset + width - 1 <= it.second.size - 2 }
                            .minByOrNull { kotlin.math.abs(it.first - tRef) } ?: continue
                        xs.add(readBe(near.second, offset, width).toDouble())
                        ys.add(vRef)
                    }
                    if (xs.size < 2) continue
                    val fit = linearFit(xs, ys) ?: continue
                    val r2 = (fit.r2 * 100).toInt().coerceIn(0, 100)
                    if (best == null || r2 > best!!.r2Pct) {
                        best = ValueProposal(
                            h.joinToString("") { "%02X".format(it) },
                            offset, width, fit.slope, fit.intercept, r2,
                        )
                    }
                }
            }
        }
        return best?.takeIf { it.r2Pct >= minR2Pct }
    }
}
