@file:OptIn(ExperimentalMaterial3Api::class)

package info.cemu.cemu.common.ui.components

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuAnchorType
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.material3.TextField
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import info.cemu.cemu.R
import info.cemu.cemu.common.ui.localization.tr

@Composable
fun <T> SelectField(
    label: String,
    choice: T,
    choices: Collection<T>,
    choiceToString: @Composable (T) -> String,
    onChoiceChanged: (T) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }

    ExposedDropdownMenuBox(
        expanded = expanded,
        onExpandedChange = { expanded = it },
        modifier = Modifier
            .fillMaxWidth()
            .padding(8.dp)
    ) {
        TextField(
            value = choiceToString(choice),
            onValueChange = {},
            readOnly = true,
            label = { Text(label) },
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
            modifier = Modifier
                .menuAnchor(ExposedDropdownMenuAnchorType.PrimaryEditable)
                .fillMaxWidth()
        )

        ExposedDropdownMenu(
            expanded = expanded,
            onDismissRequest = { expanded = false }
        ) {
            choices.forEach { selectChoice ->
                DropdownMenuItem(
                    text = { Text(choiceToString(selectChoice)) },
                    trailingIcon = {
                        if (choice == selectChoice) {
                            Icon(
                                painter = painterResource(R.drawable.ic_check),
                                tr("Current choice"),
                            )
                        }
                    },
                    onClick = {
                        onChoiceChanged(selectChoice)
                        expanded = false
                    },
                )
            }
        }
    }
}
