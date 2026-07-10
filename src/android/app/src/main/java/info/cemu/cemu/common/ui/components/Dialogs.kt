package info.cemu.cemu.common.ui.components

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.sizeIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import info.cemu.cemu.common.ui.localization.tr

@Composable
fun GenericDialog(
    title: String,
    onDismissRequest: () -> Unit,
    modifier: Modifier = Modifier,
    buttons: @Composable ColumnScope.() -> Unit = { DefaultDialogButtons(onDismissRequest) },
    content: @Composable ColumnScope.() -> Unit,
) {
    Dialog(onDismissRequest = onDismissRequest) {
        Card(
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
            modifier = modifier,
            shape = RoundedCornerShape(16.dp),
        ) {
            Text(
                modifier = Modifier.padding(start = 16.dp, end = 16.dp, top = 16.dp, bottom = 8.dp),
                text = title,
                fontSize = 24.sp,
            )

            content()

            HorizontalDivider()

            buttons()
        }
    }
}

@Composable
fun ScrollableDialog(
    title: String,
    onDismissRequest: () -> Unit,
    buttons: @Composable ColumnScope.() -> Unit = { DefaultDialogButtons(onDismissRequest) },
    content: @Composable ColumnScope.() -> Unit,
) {
    GenericDialog(
        title = title,
        onDismissRequest = onDismissRequest,
        modifier = Modifier
            .sizeIn(maxWidth = 560.dp, maxHeight = 560.dp)
            .fillMaxWidth(),
        buttons = buttons,
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 4.dp, horizontal = 16.dp)
                .weight(weight = 1.0f, fill = false)
                .verticalScroll(rememberScrollState()),
        ) {
            content()
        }
    }
}

@Composable
private fun ColumnScope.DefaultDialogButtons(onDismissRequest: () -> Unit) {
    TextButton(
        onClick = onDismissRequest,
        modifier = Modifier
            .padding(8.dp)
            .align(Alignment.End),
    ) {
        Text(tr("Cancel"))
    }
}
