package ee.zeppl.companion.ble

import android.os.SystemClock
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

/**
 * Latest DTC read/clear result pushed from the cluster (0x41), plus a busy flag
 * while a request is outstanding. Observable singleton like [TelemetryState];
 * the Diagnostics section reads it and re-renders. There is one active cluster
 * link at a time, so one shared result is enough.
 */
object DtcState {
    var reading: Boolean by mutableStateOf(false)
        private set
    var result: DtcCodec.Result? by mutableStateOf(null)
        private set
    var lastUpdatedMs: Long? by mutableStateOf(null)
        private set

    /** Send a read or clear request to the cluster and mark us busy. */
    fun sendRequest(clear: Boolean) {
        reading = true
        OutboundSink.send(Protocol.encodeDtcRequest(clear))
    }

    /** Apply a decoded 0x41 result arriving on the notify path. */
    fun apply(r: DtcCodec.Result) {
        reading = false
        result = r
        lastUpdatedMs = SystemClock.uptimeMillis()
    }
}
