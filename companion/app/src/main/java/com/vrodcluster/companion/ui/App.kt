package com.vrodcluster.companion.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.runtime.saveable.rememberSaveable

private enum class Screen { Status, AppList, Scan }

/**
 * Three-screen root. A navigation library is overkill for the
 * configuration→status→scan loop; `rememberSaveable` survives config
 * changes (rotation, dark-mode toggle) without a NavController.
 */
@Composable
fun App() {
    var screen by rememberSaveable { mutableStateOf(Screen.Status) }
    MaterialTheme {
        when (screen) {
            Screen.Status  -> StatusScreen (
                onConfigureApps = { screen = Screen.AppList },
                onPickCluster   = { screen = Screen.Scan },
            )
            Screen.AppList -> AppListScreen(onBack = { screen = Screen.Status })
            Screen.Scan    -> ScanScreen   (onBack = { screen = Screen.Status })
        }
    }
}
