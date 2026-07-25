package ee.zeppl.companion.ble

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** The guided-procedure state machine: start/confirm/skip advance + event emission. */
class ProcedureRunnerTest {

    // (stepId, action) pairs the runner emitted.
    private val events = mutableListOf<Pair<String, String>>()
    private fun runner(p: Procedure) =
        ProcedureRunner(p) { id, _, action -> events.add(id to action) }

    @Test fun `start marks the step without advancing then confirm advances`() {
        val r = runner(StandardProcedures.switchesAndLamps)
        assertEquals("left_turn", r.currentStep!!.id)
        r.start()
        assertEquals(0, r.index) // start does not advance
        r.confirm()
        assertEquals(1, r.index) // confirm advances
        assertEquals("right_turn", r.currentStep!!.id)
        assertEquals(listOf("left_turn" to "start", "left_turn" to "confirm"), events)
    }

    @Test fun `skip advances a skippable step but ignores a mandatory one`() {
        val r = runner(StandardProcedures.switchesAndLamps)
        r.skip() // left_turn is not skippable -> no-op
        assertEquals(0, r.index)
        assertTrue(events.isEmpty())

        r.confirm() // -> right_turn (mandatory)
        r.confirm() // -> hazards (skippable)
        assertEquals("hazards", r.currentStep!!.id)
        r.skip() // skippable -> advance
        assertEquals(3, r.index)
        assertEquals("hazards" to "skip", events.last())
    }

    @Test fun `completes and is a no-op past the end`() {
        val r = runner(StandardProcedures.keyOnBaseline) // one step
        assertTrue(!r.isDone)
        r.confirm()
        assertTrue(r.isDone)
        assertNull(r.currentStep)
        r.confirm() // no-op past the end
        r.start()   // no-op past the end
        assertEquals(1, events.size) // only the single confirm
    }

    @Test fun `standard procedures are non-empty and uniquely identified`() {
        val ids = StandardProcedures.all.map { it.id }
        assertEquals(ids.size, ids.toSet().size)
        StandardProcedures.all.forEach { assertTrue(it.steps.isNotEmpty()) }
    }
}
