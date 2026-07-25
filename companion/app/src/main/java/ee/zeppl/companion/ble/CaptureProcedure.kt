package ee.zeppl.companion.ble

/** Safety class: which steps are safe at standstill vs. need the bike moving. */
enum class StepSafety { STANDSTILL, RIDING }

/** How the rider is prompted — riding steps must not require looking at the phone. */
enum class StepCue { SCREEN, AUDIO, HAPTIC }

/**
 * One step of a guided capture procedure (docs/multi-vrod-adaptive-layer.md §4):
 * an instruction, an expected duration, a safety class, how the rider is cued,
 * whether it can be skipped (the bike may lack the feature), and
 * [disambiguates] — which signal the step exists to isolate, so the list can be
 * pruned intelligently and the event-track label is meaningful.
 */
data class Step(
    val id: String,
    val instruction: String,
    val expectedMs: Long,
    val safety: StepSafety,
    val cue: StepCue,
    val skippable: Boolean,
    val disambiguates: String,
)

data class Procedure(val id: String, val title: String, val steps: List<Step>) {
    init { require(steps.isNotEmpty()) { "a procedure needs at least one step" } }
}

/**
 * The built-in procedure set. Standstill-safe procedures ship first; riding
 * procedures (which need audio/haptic cues + auto-abort) come with the UI.
 */
object StandardProcedures {

    val keyOnBaseline = Procedure(
        id = "key_on_baseline",
        title = "Key-on baseline (engine off)",
        steps = listOf(
            Step(
                "key_on",
                "Turn the key ON but do NOT start the engine. Hold still.",
                8_000, StepSafety.STANDSTILL, StepCue.SCREEN, skippable = false,
                disambiguates = "baseline switch/lamp states + security handshake",
            ),
        ),
    )

    val switchesAndLamps = Procedure(
        id = "switches_lamps",
        title = "Switches & lamps",
        steps = listOf(
            Step(
                "left_turn", "Signal LEFT for 3 seconds, then off.",
                4_000, StepSafety.STANDSTILL, StepCue.SCREEN, skippable = false,
                disambiguates = "turn-signal bit order (left vs right)",
            ),
            Step(
                "right_turn", "Signal RIGHT for 3 seconds, then off.",
                4_000, StepSafety.STANDSTILL, StepCue.SCREEN, skippable = false,
                disambiguates = "turn-signal bit order (left vs right)",
            ),
            Step(
                "hazards", "Turn HAZARDS on for 3 seconds, then off.",
                4_000, StepSafety.STANDSTILL, StepCue.SCREEN, skippable = true,
                disambiguates = "both-turn (hazard) encoding",
            ),
            Step(
                "high_beam", "Switch to HIGH beam for 3 seconds, then back to low.",
                4_000, StepSafety.STANDSTILL, StepCue.SCREEN, skippable = true,
                disambiguates = "high-beam signal (bus bit or discrete)",
            ),
        ),
    )

    val all = listOf(keyOnBaseline, switchesAndLamps)
}
