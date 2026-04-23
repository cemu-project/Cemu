package info.cemu.cemu.emulation.emulatedusbdevices

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import info.cemu.cemu.common.ui.components.GenericDialog
import info.cemu.cemu.common.ui.localization.tr

@Composable
fun FigureCreateDialog(
    title: String,
    onDismiss: () -> Unit,
    onCreate: () -> Unit,
    content: @Composable ColumnScope.() -> Unit
) {
    GenericDialog(
        title = title,
        onDismissRequest = onDismiss,
        buttons = {
            Row(modifier = Modifier.align(Alignment.End)) {
                TextButton(onClick = onDismiss) { Text("Cancel") }
                TextButton(onClick = {
                    onCreate()
                    onDismiss()
                }) { Text(tr("Create")) }
            }
        },
    ) {
        Column(
            modifier = Modifier.padding(8.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            content()
        }
    }
}