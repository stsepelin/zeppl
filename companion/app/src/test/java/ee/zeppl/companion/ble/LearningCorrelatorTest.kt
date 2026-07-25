package ee.zeppl.companion.ble

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

/**
 * The toggles-with-switch detector. Given a labelled dump, it should recover the
 * exact bit that tracks the switch — the same result the manual "toggle + diff"
 * mapping produced for the turn signals (48 DA 40, data byte offset 4, bit1).
 */
class LearningCorrelatorTest {

    private fun container(
        frames: List<CaptureFrame>,
        events: List<CaptureEvent>,
    ) = CaptureContainer(metadata = CaptureMetadata(), frames = frames, events = events)

    @Test fun `recovers the bit that tracks a switch`() {
        // Left-turn ON window [100, 200]; the turn byte (offset 4) has bit1 set
        // only inside it. 48 DA 40 39 <turn> <crc>.
        val c = container(
            frames = listOf(
                CaptureFrame(50, "48DA40390070"),  // off, before
                CaptureFrame(150, "48DA4039024A"), // on (bit1), inside
                CaptureFrame(250, "48DA40390070"), // off, after
            ),
            events = listOf(
                CaptureEvent(100, "left_turn", "Signal left", "start"),
                CaptureEvent(200, "left_turn", "Signal left", "confirm"),
            ),
        )
        val p = LearningCorrelator.proposeFlag(c, "left_turn")!!
        assertEquals("48DA40", p.header)
        assertEquals(4, p.offset)
        assertEquals(0x02, p.mask)
        assertEquals(100, p.confidencePct)
    }

    @Test fun `null when the step was never windowed`() {
        val c = container(
            frames = listOf(CaptureFrame(150, "48DA4039024A")),
            events = listOf(
                CaptureEvent(100, "left_turn", "Signal left", "start"),
                CaptureEvent(200, "left_turn", "Signal left", "confirm"),
            ),
        )
        assertNull(LearningCorrelator.proposeFlag(c, "right_turn")) // no windows for this step
    }

    @Test fun `null when nothing tracks well enough`() {
        // The turn byte is constant across the window -> no bit correlates >= 90%.
        val c = container(
            frames = listOf(
                CaptureFrame(50, "48DA40390000"),
                CaptureFrame(150, "48DA40390000"),
                CaptureFrame(250, "48DA40390000"),
            ),
            events = listOf(
                CaptureEvent(100, "left_turn", "Signal left", "start"),
                CaptureEvent(200, "left_turn", "Signal left", "confirm"),
            ),
        )
        assertNull(LearningCorrelator.proposeFlag(c, "left_turn"))
    }

    @Test fun `null when there are no usable frames`() {
        val c = container(
            frames = emptyList(),
            events = listOf(
                CaptureEvent(100, "left_turn", "Signal left", "start"),
                CaptureEvent(200, "left_turn", "Signal left", "confirm"),
            ),
        )
        assertNull(LearningCorrelator.proposeFlag(c, "left_turn"))
    }

    @Test fun `handles multiple on windows for the same step`() {
        val c = container(
            frames = listOf(
                CaptureFrame(50, "48DA40390070"),  // off
                CaptureFrame(150, "48DA4039024A"), // window 1
                CaptureFrame(250, "48DA40390070"), // off
                CaptureFrame(350, "48DA4039024A"), // window 2
                CaptureFrame(450, "48DA40390070"), // off
            ),
            events = listOf(
                CaptureEvent(100, "left_turn", "Signal left", "start"),
                CaptureEvent(200, "left_turn", "Signal left", "confirm"),
                CaptureEvent(300, "left_turn", "Signal left", "start"),
                CaptureEvent(400, "left_turn", "Signal left", "confirm"),
            ),
        )
        val p = LearningCorrelator.proposeFlag(c, "left_turn")!!
        assertEquals(0x02, p.mask)
        assertEquals(100, p.confidencePct)
    }

    // --- continuous value detector (linear fit from reference points) --------

    @Test fun `recovers a linear value scale from reference points`() {
        fun rpm(raw: Int) = "281B1002%04X00".format(raw) // 28 1B 10 02 HH LL + dummy CRC
        val c = container(
            frames = listOf(
                CaptureFrame(100, rpm(4001)),
                CaptureFrame(200, rpm(8003)),
                CaptureFrame(300, rpm(2002)),
            ),
            events = emptyList(),
        )
        // engineering = raw * 0.25 (the RPM /4 relationship), given as reference points.
        val ref = listOf(100L to 1000.25, 200L to 2000.75, 300L to 500.5)
        val p = LearningCorrelator.proposeValue(c, ref)!!
        assertEquals("281B10", p.header)
        assertEquals(4, p.offset)
        assertEquals(2, p.width)
        assertEquals(0.25, p.scale, 1e-6)
        assertEquals(0.0, p.bias, 1e-3)
        assertEquals(100, p.r2Pct)
    }

    @Test fun `null with fewer than two reference points`() {
        val c = container(listOf(CaptureFrame(100, "281B1002123400")), emptyList())
        assertNull(LearningCorrelator.proposeValue(c, listOf(100L to 1.0)))
    }

    @Test fun `null when no field fits the reference`() {
        // A constant field cannot correlate with a varying reference.
        val c = container(
            frames = listOf(
                CaptureFrame(100, "281B1002000000"),
                CaptureFrame(200, "281B1002000000"),
            ),
            events = emptyList(),
        )
        assertNull(LearningCorrelator.proposeValue(c, listOf(100L to 10.0, 200L to 20.0)))
    }
}
