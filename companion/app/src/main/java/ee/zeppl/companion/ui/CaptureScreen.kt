package ee.zeppl.companion.ui

import android.content.Intent
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import ee.zeppl.companion.ble.BleConnState
import ee.zeppl.companion.ble.BleState
import ee.zeppl.companion.ble.CaptureMetadata
import ee.zeppl.companion.ble.CaptureState
import ee.zeppl.companion.ble.ProcedureRunner
import ee.zeppl.companion.ble.StandardProcedures
import ee.zeppl.companion.ble.StepSafety

/**
 * Guided bus-capture screen for the multi-V-Rod adaptive layer
 * (docs/multi-vrod-adaptive-layer.md §4). Walks the rider through a labelled
 * procedure while the cluster streams raw frames (a CONFIG_VROD_J1850_RAW_SNIFF
 * capture build); the finished dump is shared for profile building.
 *
 * Not unit-tested (Compose UI); the logic it drives — ProcedureRunner,
 * CaptureState, CaptureContainer — is covered by JVM tests.
 */
@Composable
fun CaptureScreen(onBack: () -> Unit) {
    val context = LocalContext.current

    var model by remember { mutableStateOf("") }
    var year by remember { mutableStateOf("") }
    var selected by remember { mutableStateOf(StandardProcedures.all.first()) }
    var runner by remember { mutableStateOf<ProcedureRunner?>(null) }
    var stepIndex by remember { mutableStateOf(0) }
    var lastExportBytes by remember { mutableStateOf<Int?>(null) }

    val active = CaptureState.active
    val connected = BleState.conn == BleConnState.CONNECTED

    // Auto-abort if the link drops mid-capture: a partial dump is still valid,
    // but we stop recording so the event track stays truthful.
    LaunchedEffect(BleState.conn) {
        if (CaptureState.active && BleState.conn != BleConnState.CONNECTED) {
            CaptureState.stop()
            runner = null
        }
    }

    fun startCapture() {
        CaptureState.start(
            CaptureMetadata(model = model.ifBlank { null }, yearRange = year.ifBlank { null }),
        )
        val r = ProcedureRunner(selected) { id, label, action -> CaptureState.label(id, label, action) }
        r.start()
        runner = r
        stepIndex = 0
    }

    fun advance(skip: Boolean) {
        val r = runner ?: return
        if (skip) r.skip() else r.confirm()
        stepIndex = r.index
        if (!r.isDone) r.start()
    }

    fun finish() {
        val json = CaptureState.stop()
        lastExportBytes = json?.toByteArray()?.size
        runner = null
        if (json != null) {
            val intent = Intent(Intent.ACTION_SEND).apply {
                type = "application/json"
                putExtra(Intent.EXTRA_SUBJECT, "Zeppl capture dump")
                putExtra(Intent.EXTRA_TEXT, json)
            }
            context.startActivity(Intent.createChooser(intent, "Share capture dump"))
        }
    }

    ScreenColumn(title = "Guided capture", subtitle = "Record a labelled bus dump for profile building") {
        if (!active) {
            SectionCard(title = "Bike") {
                OutlinedTextField(
                    value = model, onValueChange = { model = it },
                    label = { Text("Model (e.g. VRSCF)") }, singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                Spacer(Modifier.height(8.dp))
                OutlinedTextField(
                    value = year, onValueChange = { year = it },
                    label = { Text("Year") }, singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
            }
            SectionCard(title = "Procedure") {
                StandardProcedures.all.forEach { p ->
                    if (p.id == selected.id) {
                        Button(onClick = { selected = p }, modifier = Modifier.fillMaxWidth()) { Text(p.title) }
                    } else {
                        OutlinedButton(onClick = { selected = p }, modifier = Modifier.fillMaxWidth()) { Text(p.title) }
                    }
                    Spacer(Modifier.height(6.dp))
                }
                Text(
                    "${selected.steps.size} steps",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Button(
                onClick = { startCapture() },
                enabled = connected,
                modifier = Modifier.fillMaxWidth(),
            ) { Text("Start capture") }
            if (!connected) {
                Text(
                    "Connect to the cluster (running a raw-sniff capture build) first.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            lastExportBytes?.let {
                Text("Last dump: $it bytes shared.", style = MaterialTheme.typography.bodySmall)
            }
        } else {
            SectionCard(title = "Recording") {
                InfoRow("frames", CaptureState.frameCount.toString())
                InfoRow("dropped", CaptureState.droppedFrames.toString())
            }
            val step = selected.steps.getOrNull(stepIndex)
            if (step != null) {
                SectionCard(title = "Step ${stepIndex + 1} of ${selected.steps.size}") {
                    Text(step.instruction, style = MaterialTheme.typography.bodyLarge)
                    Spacer(Modifier.height(4.dp))
                    Text(
                        if (step.safety == StepSafety.RIDING) {
                            "While riding — keep your eyes on the road"
                        } else {
                            "At a standstill"
                        },
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.primary,
                    )
                    Spacer(Modifier.height(8.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
                        Button(onClick = { advance(skip = false) }, modifier = Modifier.weight(1f)) { Text("Did it") }
                        if (step.skippable) {
                            OutlinedButton(onClick = { advance(skip = true) }, modifier = Modifier.weight(1f)) {
                                Text("Skip")
                            }
                        }
                    }
                }
            } else {
                SectionCard(title = "Done") {
                    Text("All steps complete. Export the dump to share it for profile building.")
                    Spacer(Modifier.height(8.dp))
                    Button(onClick = { finish() }, modifier = Modifier.fillMaxWidth()) { Text("Finish & export") }
                }
            }
            OutlinedButton(
                onClick = {
                    CaptureState.stop()
                    runner = null
                },
                modifier = Modifier.fillMaxWidth(),
            ) { Text("Cancel capture") }
        }

        OutlinedButton(onClick = onBack, modifier = Modifier.fillMaxWidth()) { Text("Back") }
    }
}
