package info.cemu.cemu.emulation

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.nativeinterface.NativeSwkbd

@Composable
fun EmulationTextInputDialog() {
    val swkbdState = NativeSwkbd.SwkbdState.data.collectAsState()

    val swkbdStateData = swkbdState.value

    fun onDone() {
        if (swkbdStateData?.text?.isNotEmpty() == true) {
            NativeSwkbd.onFinishedInputEdit()
        }
    }

    if (swkbdStateData != null) {
        Dialog(onDismissRequest = {}) {
            Card(
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
                shape = RoundedCornerShape(16.dp),
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(16.dp)
            ) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    OutlinedTextField(
                        value = swkbdStateData.text,
                        placeholder = { Text(tr("Input text")) },
                        onValueChange = { NativeSwkbd.SwkbdState.updateText(it) },
                        keyboardOptions = KeyboardOptions(imeAction = ImeAction.Done),
                        keyboardActions = KeyboardActions(onDone = { onDone() }),
                        singleLine = true,
                        supportingText = {
                            if (swkbdStateData.maxLength > 0) {
                                Text(
                                    text = "${swkbdStateData.text.length} / ${swkbdStateData.maxLength}",
                                    modifier = Modifier.fillMaxWidth(),
                                    textAlign = TextAlign.End
                                )
                            }
                        },
                    )
                    TextButton(
                        modifier = Modifier.align(Alignment.End),
                        enabled = swkbdStateData.text.isNotEmpty(),
                        onClick = { onDone() },
                    ) {
                        Text(tr("Done"))
                    }
                }
            }
        }
    }
}
