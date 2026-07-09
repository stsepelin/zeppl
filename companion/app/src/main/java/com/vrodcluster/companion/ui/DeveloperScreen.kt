package com.vrodcluster.companion.ui

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import com.vrodcluster.companion.ble.TelemetryState

/**
 * Gated developer tools, unlocked by tapping the Firmware row on the Cluster
 * page seven times. Hosts the speed-calibration wizard (correlate GPS against
 * the raw ECM count, write the divisor back to the cluster) and a raw-telemetry
 * readout for debugging the decode.
 */
@Composable
fun DeveloperScreen(onExit: () -> Unit) {
    ScreenColumn(title = "Developer", subtitle = "Calibration, config, and raw telemetry") {
        SpeedCalibrationCard()

        SectionCard(title = "Live telemetry (raw)") {
            InfoRow("speed_raw", TelemetryState.speedRaw?.toString() ?: DASH)
            InfoRow("speed_mph", TelemetryState.speedMph?.toString() ?: DASH)
            InfoRow("rpm", TelemetryState.rpm?.toString() ?: DASH)
            InfoRow("gear", TelemetryState.gear?.toString() ?: DASH)
            InfoRow("engine_temp_c", TelemetryState.engineTempC?.toString() ?: DASH)
            InfoRow("fuel_level", TelemetryState.fuelLevel?.toString() ?: DASH)
            InfoRow("lamps", TelemetryState.lamps?.let { "0x%03X".format(it) } ?: DASH)
            InfoRow("odometer_m", TelemetryState.odometerM?.toString() ?: DASH)
            InfoRow("trip1_fuel", TelemetryState.trip1FuelTicks?.toString() ?: DASH)
            InfoRow("last_frame_ms", TelemetryState.lastFrameMs?.toString() ?: DASH)
        }

        OutlinedButton(onClick = onExit, modifier = Modifier.fillMaxWidth()) {
            Text("Disable developer mode")
        }
    }
}

private const val DASH = "—"
