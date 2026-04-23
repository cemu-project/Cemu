package info.cemu.cemu.common.ui.extensions

import androidx.compose.material3.SnackbarHostState
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch

fun SnackbarHostState.showMessage(scope: CoroutineScope, message: String) = scope.launch {
    currentSnackbarData?.dismiss()
    showSnackbar(message)
}
