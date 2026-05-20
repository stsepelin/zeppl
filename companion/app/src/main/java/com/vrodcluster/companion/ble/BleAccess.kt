package com.vrodcluster.companion.ble

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import androidx.core.content.ContextCompat

/**
 * Runtime-permission helpers for the BLE central role.
 *
 * On Android 12+ we need BLUETOOTH_SCAN and BLUETOOTH_CONNECT; on
 * Android 13+ POST_NOTIFICATIONS is required so the foreground-service
 * banner can render. The manifest declares them, but the user has to
 * grant the runtime equivalents the first time we connect.
 */
object BleAccess {

    /** The exact set we request together at first-connect time. */
    val REQUIRED: Array<String> = arrayOf(
        Manifest.permission.BLUETOOTH_SCAN,
        Manifest.permission.BLUETOOTH_CONNECT,
        Manifest.permission.POST_NOTIFICATIONS,
    )

    fun allGranted(context: Context): Boolean =
        REQUIRED.all { p ->
            ContextCompat.checkSelfPermission(context, p) == PackageManager.PERMISSION_GRANTED
        }
}
