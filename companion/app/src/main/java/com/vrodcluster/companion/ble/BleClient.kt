package com.vrodcluster.companion.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.ParcelUuid
import android.util.Log

/**
 * Scans for a peripheral advertising [Protocol.SERVICE_UUID], connects
 * to the first match, requests a 247-byte MTU (enough for the largest
 * NOTIF payload in one ATT write), then routes every byte handed to
 * it via [write] to the RX characteristic.
 *
 * Callbacks (scanner + GATT) all fire on the main looper of the
 * binder thread the framework hands us; the foreground service is
 * already on main, so we don't add any synchronisation here.
 *
 * Permission annotations: minSdk 36 means BLUETOOTH_SCAN / _CONNECT
 * are runtime; [BleAccess.allGranted] guards entry points before
 * this class is touched.
 */
class BleClient(private val appContext: Context) {

    private val adapter   = appContext.getSystemService(BluetoothManager::class.java).adapter
    private var gatt:      BluetoothGatt?               = null
    private var writeChar: BluetoothGattCharacteristic? = null

    private val scanCallback = object : ScanCallback() {
        @SuppressLint("MissingPermission")
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            // First match wins; stop scanning immediately so we don't
            // keep the radio hot. If the user has multiple clusters
            // they can disambiguate via deviceName later.
            adapter.bluetoothLeScanner?.stopScan(this)
            Log.i(TAG, "scan hit: ${result.device.address} (${result.device.name})")
            connectTo(result.device)
        }

        override fun onScanFailed(errorCode: Int) {
            Log.w(TAG, "scan failed: $errorCode")
            BleState.conn = BleConnState.IDLE
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {

        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    Log.i(TAG, "gatt connected; requesting MTU 247")
                    // Larger MTU = full NOTIF payload (~150B incl. UTF-8
                    // sender + 128B message) in one ATT write. The
                    // cluster supports LE-2M but won't reject a smaller
                    // negotiated MTU if the link can't sustain it.
                    g.requestMtu(247)
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    Log.i(TAG, "gatt disconnected; status=$status")
                    writeChar = null
                    g.close()
                    if (gatt === g) gatt = null
                    BleState.conn = BleConnState.DISCONNECTED
                }
            }
        }

        @SuppressLint("MissingPermission")
        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            Log.i(TAG, "mtu=$mtu status=$status — discovering services")
            g.discoverServices()
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val svc = g.getService(Protocol.SERVICE_UUID)
            val chr = svc?.getCharacteristic(Protocol.RX_UUID)
            if (chr == null) {
                Log.w(TAG, "RX characteristic not found on device")
                BleState.conn = BleConnState.DISCONNECTED
                return
            }
            writeChar = chr
            BleState.conn = BleConnState.CONNECTED
            Log.i(TAG, "ready; RX handle resolved")
        }
    }

    // --- public API ----------------------------------------------------

    @SuppressLint("MissingPermission")
    fun start() {
        if (BleState.conn != BleConnState.IDLE && BleState.conn != BleConnState.DISCONNECTED) return
        val scanner = adapter?.bluetoothLeScanner ?: run {
            Log.w(TAG, "no BLE scanner — bluetooth probably off")
            BleState.conn = BleConnState.IDLE
            return
        }
        val filter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid(Protocol.SERVICE_UUID))
            .build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        BleState.conn = BleConnState.SCANNING
        scanner.startScan(listOf(filter), settings, scanCallback)
        Log.i(TAG, "scanning for cluster")
    }

    @SuppressLint("MissingPermission")
    fun stop() {
        adapter?.bluetoothLeScanner?.stopScan(scanCallback)
        gatt?.disconnect()
        gatt?.close()
        gatt = null
        writeChar = null
        BleState.deviceName = null
        BleState.conn = BleConnState.IDLE
    }

    @SuppressLint("MissingPermission")
    fun write(bytes: ByteArray) {
        val g = gatt ?: return
        val c = writeChar ?: return
        // WRITE_NO_RESPONSE matches the firmware characteristic flags
        // (BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP) and
        // keeps us off the slow request/response loop for things like
        // PlaybackState position updates.
        val rc = g.writeCharacteristic(
            c, bytes, BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        )
        BleState.lastTx = bytes.joinToString(" ") { "%02X".format(it) }
        if (rc != android.bluetooth.BluetoothStatusCodes.SUCCESS) {
            Log.w(TAG, "writeCharacteristic rc=$rc")
        }
    }

    // --- internals -----------------------------------------------------

    @SuppressLint("MissingPermission")
    private fun connectTo(device: BluetoothDevice) {
        BleState.deviceName = device.name ?: device.address
        BleState.conn = BleConnState.CONNECTING
        gatt = device.connectGatt(appContext, /*autoConnect=*/false, gattCallback)
    }

    private companion object {
        const val TAG = "BleClient"
    }
}
