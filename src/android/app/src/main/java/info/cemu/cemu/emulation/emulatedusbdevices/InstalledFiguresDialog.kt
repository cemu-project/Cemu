package info.cemu.cemu.emulation.emulatedusbdevices

import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import info.cemu.cemu.R
import info.cemu.cemu.common.ui.components.ScrollableDialog
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.nativeinterface.NativeEmulatedUSBDevices

@Composable
fun InstalledFiguresDialog(
    label: String,
    installedFigures: Array<NativeEmulatedUSBDevices.InstalledFigure>,
    onDelete: (NativeEmulatedUSBDevices.InstalledFigure) -> Unit,
    onDismiss: () -> Unit,
) {
    ScrollableDialog(
        title = label, onDismissRequest = onDismiss
    ) {
        if (installedFigures.isEmpty()) {
            Text(
                text = tr("No available figures"),
                fontSize = 18.sp,
                modifier = Modifier
                    .align(Alignment.CenterHorizontally)
                    .padding(8.dp),
            )
            return@ScrollableDialog
        }

        installedFigures.forEach { figure ->
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(figure.name, modifier = Modifier.weight(1f))
                IconButton(onClick = { onDelete(figure) }) {
                    Icon(
                        painter = painterResource(R.drawable.ic_delete),
                        contentDescription = null
                    )
                }
            }
        }
    }
}