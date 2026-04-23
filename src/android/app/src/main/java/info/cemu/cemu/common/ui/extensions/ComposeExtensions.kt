package info.cemu.cemu.common.ui.extensions

import android.text.format.Formatter
import androidx.compose.runtime.Composable
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.ui.platform.LocalContext

@ReadOnlyComposable
@Composable
fun Long.formatBytes(): String {
    val context = LocalContext.current
    return Formatter.formatFileSize(context, this)
}
