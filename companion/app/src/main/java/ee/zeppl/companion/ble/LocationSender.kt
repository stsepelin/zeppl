package ee.zeppl.companion.ble

import android.Manifest
import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.PackageManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Looper
import androidx.core.content.ContextCompat

/**
 * Streams the phone's GPS position to the cluster for the map view - the cluster
 * has no GPS of its own. Registered for the life of the BLE link ([BleService]),
 * it sends a LOCATION packet on each fix (~1 Hz, no min displacement) while
 * fine-location is granted, so the cluster map has a steady position even when
 * crawling or stopped (a min-distance filter used to starve it at low speed).
 * Heading comes from the fix's bearing when moving, else unknown (Android only
 * reports bearing from motion, so a stationary map stays north-up).
 *
 * Background delivery (screen off, app backgrounded) needs the service's
 * LOCATION foreground-service type, which [BleService] adds whenever fine
 * location is granted.
 */
class LocationSender(context: Context) {

    private val app = context.applicationContext
    private val lm  = app.getSystemService(Context.LOCATION_SERVICE) as LocationManager

    private val listener = object : LocationListener {
        override fun onLocationChanged(location: Location) {
            val headingCd = if (location.hasBearing()) {
                ((location.bearing * 100).toInt()).mod(36000)
            } else {
                Protocol.HEADING_UNKNOWN
            }
            OutboundSink.send(Protocol.encodeLocation(location.latitude, location.longitude, headingCd))
        }
    }
    private var registered = false

    @SuppressLint("MissingPermission")
    fun start() {
        val granted = ContextCompat.checkSelfPermission(
            app, Manifest.permission.ACCESS_FINE_LOCATION,
        ) == PackageManager.PERMISSION_GRANTED
        if (granted && !registered) {
            try {
                lm.requestLocationUpdates(
                    LocationManager.GPS_PROVIDER, 1000L, 0f, listener, Looper.getMainLooper(),
                )
                registered = true
            } catch (_: SecurityException) {
            }
        }
    }

    fun stop() {
        if (registered) {
            lm.removeUpdates(listener)
            registered = false
        }
    }
}
