@file:OptIn(ExperimentalMaterial3Api::class)

package info.cemu.cemu.common.ui.components

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuAnchorType
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextField
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier

@Composable
fun <T> SearchableDropdown(
    label: String,
    choices: Array<T>,
    choiceToString: (T) -> String,
    onChoiceSelected: (T) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    var text by remember { mutableStateOf("") }
    val filteredItems = remember(text) {
        if (text.isEmpty()) {
            return@remember choices.toList()
        }

        return@remember choices.filter { choiceToString(it).contains(text, ignoreCase = true) }
    }

    ExposedDropdownMenuBox(
        expanded = expanded,
        modifier = Modifier.fillMaxWidth(),
        onExpandedChange = { expanded = it }) {
        TextField(
            value = text,
            label = { Text(label) },
            onValueChange = { text = it },
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
            modifier = Modifier
                .menuAnchor(ExposedDropdownMenuAnchorType.PrimaryEditable)
                .fillMaxWidth(),
        )
        ExposedDropdownMenu(
            expanded = expanded, onDismissRequest = { expanded = false }) {
            filteredItems.forEach { item ->
                DropdownMenuItem(
                    text = { Text(choiceToString(item)) },
                    onClick = {
                        text = choiceToString(item)
                        onChoiceSelected(item)
                        expanded = false
                    },
                )
            }
        }
    }
}