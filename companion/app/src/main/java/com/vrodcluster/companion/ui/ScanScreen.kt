package com.vrodcluster.companion.ui

import android.bluetooth.BluetoothDevice
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.vrodcluster.companion.ble.BleScanner
import com.vrodcluster.companion.ble.BleService
import com.vrodcluster.companion.ble.BondTracker

/**
 * Active BLE scan with a tap-to-connect device list. Replaces the
 * "Connect Cluster" first-match-wins shortcut with explicit picking,
 * useful when:
 *   - multiple clusters are in range (bench / development),
 *   - the phone-side bond is stale and Samsung's BT settings hides
 *     the device from the OS pair-new-device list,
 *   - the rider wants visual confirmation (RSSI) of which cluster
 *     they're attaching to before committing.
 *
 * Bond-state from [BondTracker] is shown below so the user sees
 * "Pairing…" / "Paired" once they tap a device — without it the UI
 * can look frozen during the Android system pairing dialog.
 */
@Composable
fun ScanScreen(onBack: () -> Unit) {
    val context = LocalContext.current
    val found = remember { mutableStateMapOf<String, BleScanner.ScannedDevice>() }
    var scanning by remember { mutableStateOf(true) }

    // Start the scanner on enter, stop on leave. The scanner lives for
    // the lifetime of the composable so a recomposition doesn't restart
    // it and reset the RSSI ordering.
    val scanner = remember {
        BleScanner(context.applicationContext) { dev ->
            // Last-write-wins per MAC. RSSI bounces; the user sees the
            // most recent value (more useful than averaging when picking
            // among nearby devices).
            found[dev.address] = dev
        }
    }
    DisposableEffect(Unit) {
        scanner.start()
        BondTracker.start(context.applicationContext)
        onDispose {
            scanner.stop()
            // Leave BondTracker running across screens — connection state
            // can change while the user is on the status screen, and the
            // listener is cheap.
        }
    }

    // Restart the scan if the user toggles back on after stopping it
    // (e.g., tapped Stop and changed their mind).
    LaunchedEffect(scanning) {
        if (scanning) scanner.start() else scanner.stop()
    }

    Scaffold { pad ->
        Column(
            Modifier.fillMaxSize().padding(pad).padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text("Pick a cluster",
                 style = MaterialTheme.typography.headlineSmall)

            Text(
                bondStatusText(),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )

            Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.Center) {
                if (found.isEmpty() && scanning) {
                    Text("Scanning…",
                         style = MaterialTheme.typography.bodyMedium,
                         color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }

            // The list grows as devices come in. Sort by signal so the
            // closest cluster is on top — usually the one the rider
            // wants.
            val sorted = found.values.sortedByDescending { it.rssi }
            LazyColumn(
                Modifier.fillMaxWidth(),
                contentPadding = PaddingValues(vertical = 4.dp),
                verticalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                items(sorted, key = { it.address }) { dev ->
                    DeviceRow(dev) {
                        scanner.stop()
                        scanning = false
                        BleService.start(context, address = dev.address)
                        onBack()
                    }
                    HorizontalDivider()
                }
            }

            // Bottom controls.
            Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.CenterEnd) {
                Column(horizontalAlignment = Alignment.End) {
                    Button(onClick = { scanning = !scanning }) {
                        Text(if (scanning) "Stop scan" else "Restart scan")
                    }
                    TextButton(onClick = onBack) { Text("Cancel") }
                }
            }
        }
    }
}

@Composable
private fun DeviceRow(dev: BleScanner.ScannedDevice, onConnect: () -> Unit) {
    Box(Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Column(Modifier.fillMaxWidth()) {
            Text(dev.name ?: "(unnamed)",
                 style = MaterialTheme.typography.bodyLarge)
            Text("${dev.address} • RSSI ${dev.rssi} dBm",
                 style = MaterialTheme.typography.bodySmall,
                 color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.CenterEnd) {
            Button(onClick = onConnect) { Text("Connect") }
        }
    }
}

@Composable
private fun bondStatusText(): String {
    val state = BondTracker.bondState
    val addr  = BondTracker.bondedAddress
    return when (state) {
        android.bluetooth.BluetoothDevice.BOND_BONDING -> "Pairing with ${addr ?: "device"}… confirm the matching code on the cluster"
        android.bluetooth.BluetoothDevice.BOND_BONDED  -> "Paired with ${addr ?: "device"}"
        else                                            -> ""
    }
}
