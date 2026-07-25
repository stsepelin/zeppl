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
}
