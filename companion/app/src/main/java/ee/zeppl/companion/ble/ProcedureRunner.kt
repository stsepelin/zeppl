package ee.zeppl.companion.ble

/**
 * Drives a [Procedure] step by step, emitting a labelled event for each rider
 * action (start / confirm / skip) so the capture's event track lines up with the
 * bus frames. Pure state machine — the emitter is a callback (wired to
 * `CaptureState.label` in the UI), so the runner is testable on its own.
 */
class ProcedureRunner(
    val procedure: Procedure,
    private val onEvent: (stepId: String, label: String, action: String) -> Unit,
) {
    var index = 0
        private set

    val currentStep: Step? get() = procedure.steps.getOrNull(index)
    val isDone: Boolean get() = index >= procedure.steps.size

    /** Rider begins the current step's action — marks the window start. No advance. */
    fun start() {
        val s = currentStep ?: return
        onEvent(s.id, s.disambiguates, "start")
    }

    /** Rider confirms the step is done; advance to the next. */
    fun confirm() {
        val s = currentStep ?: return
        onEvent(s.id, s.disambiguates, "confirm")
        index++
    }

    /** Rider skips (bike lacks the feature); advance. A non-skippable step ignores this. */
    fun skip() {
        val s = currentStep ?: return
        if (!s.skippable) return
        onEvent(s.id, s.disambiguates, "skip")
        index++
    }
}
