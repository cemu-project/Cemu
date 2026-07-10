package info.cemu.cemu.common.ui.components

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Text
import androidx.compose.material3.TextField
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType

@Composable
private fun NumericFieldCore(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    minValue: Long = Long.MIN_VALUE,
    maxValue: Long = Long.MAX_VALUE,
) {
    var text by remember { mutableStateOf(value) }

    LaunchedEffect(value) {
        if (text.isEmpty() && value == "0") {
            return@LaunchedEffect
        }

        val currentNumber = text.toLongOrNull()

        if (currentNumber != null && currentNumber.toString() == value) {
            return@LaunchedEffect
        }

        text = value
    }

    TextField(
        value = text,
        label = { Text(label) },
        modifier = Modifier.fillMaxWidth(),
        onValueChange = { newValue ->
            if (newValue.isEmpty()) {
                text = newValue
                onValueChange("0")
                return@TextField
            }

            val number = newValue.toLongOrNull() ?: return@TextField

            if (number !in minValue..maxValue) {
                return@TextField
            }

            text = newValue
            onValueChange(newValue)
        },
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
    )
}

@Composable
fun NumericUIntField(
    label: String,
    value: UInt,
    minValue: UInt = 0u,
    maxValue: UInt = UInt.MAX_VALUE,
    onValueChanged: (UInt) -> Unit,
) = NumericFieldCore(
    label = label,
    value = value.toString(),
    minValue = minValue.toLong(),
    maxValue = maxValue.toLong(),
    onValueChange = { newValue -> onValueChanged(newValue.toUInt()) }
)


@Composable
fun NumericUShortField(
    label: String,
    value: UShort,
    minValue: UShort = 0u,
    maxValue: UShort = UShort.MAX_VALUE,
    onValueChanged: (UShort) -> Unit,
) = NumericFieldCore(
    label = label,
    value = value.toString(),
    minValue = minValue.toLong(),
    maxValue = maxValue.toLong(),
    onValueChange = { newValue -> onValueChanged(newValue.toUShort()) }
)
