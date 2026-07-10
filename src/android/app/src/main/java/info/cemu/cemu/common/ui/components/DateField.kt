@file:OptIn(ExperimentalMaterial3Api::class)

package info.cemu.cemu.common.ui.components

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.DatePicker
import androidx.compose.material3.DatePickerDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TextField
import androidx.compose.material3.rememberDatePickerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import info.cemu.cemu.R
import info.cemu.cemu.common.ui.localization.LocalLocale
import info.cemu.cemu.common.ui.localization.tr
import java.text.SimpleDateFormat
import java.util.Date

@Composable
fun DateField(
    label: String,
    dateMillis: Long,
    onDateChange: (Long) -> Unit
) {
    var showDatePicker by remember { mutableStateOf(false) }

    TextField(
        value = convertMillisToDate(dateMillis),
        onValueChange = {},
        readOnly = true,
        label = { Text(label) },
        trailingIcon = {
            IconButton(onClick = { showDatePicker = !showDatePicker }) {
                Icon(painterResource(R.drawable.ic_date_range), tr("Pick a date"))
            }
        },
        modifier = Modifier
            .fillMaxWidth()
            .padding(8.dp),
    )

    if (showDatePicker) {
        DatePickerModal(
            initialDateMillis = dateMillis,
            onDateChanged = onDateChange,
            onDismiss = { showDatePicker = false },
        )
    }
}

@Composable
private fun DatePickerModal(
    initialDateMillis: Long,
    onDateChanged: (Long) -> Unit,
    onDismiss: () -> Unit,
) {
    val datePickerState = rememberDatePickerState(initialSelectedDateMillis = initialDateMillis)

    DatePickerDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = {
                onDateChanged(datePickerState.selectedDateMillis ?: 0)
                onDismiss()
            }) {
                Text(tr("OK"))
            }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text(tr("Cancel")) } }
    ) {
        DatePicker(state = datePickerState)
    }
}

@Composable
private fun convertMillisToDate(millis: Long): String {
    val locale = LocalLocale.current
    val formatter = SimpleDateFormat("yyyy-MM-dd", locale)
    return formatter.format(Date(millis))
}
