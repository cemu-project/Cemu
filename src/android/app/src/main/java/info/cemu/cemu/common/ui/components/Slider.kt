package info.cemu.cemu.common.ui.components

import androidx.annotation.IntRange
import androidx.compose.foundation.LocalIndication
import androidx.compose.foundation.focusable
import androidx.compose.foundation.indication
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt
import androidx.compose.material3.Slider as MaterialSlider

@Composable
fun Slider(
    label: String,
    value: Int,
    valueFrom: Int,
    valueTo: Int,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    @IntRange(from = 0) steps: Int = (valueTo - valueFrom - 1).coerceAtLeast(0),
    labelFormatter: (Int) -> String,
    onValueChange: (Int) -> Unit,
) {
    var sliderValue by rememberSaveable(value) { mutableFloatStateOf(value.toFloat()) }
    val interactionSource = remember { MutableInteractionSource() }
    val indication = LocalIndication.current

    Column(
        modifier = modifier
            .focusable(
                enabled = enabled,
                interactionSource = interactionSource
            )
            .indication(
                interactionSource = interactionSource,
                indication = if (enabled) indication else null
            )
            .padding(8.dp)
            .alpha(if (enabled) 1f else 0.6f)
    ) {
        Text(
            modifier = Modifier.padding(bottom = 8.dp),
            text = label,
            fontWeight = FontWeight.Medium,
            fontSize = 16.sp,
        )
        Text(
            modifier = Modifier.padding(top = 8.dp),
            text = labelFormatter(sliderValue.roundToInt()),
            fontWeight = FontWeight.Light,
            fontSize = 14.sp,
        )
        MaterialSlider(
            valueRange = valueFrom.toFloat()..valueTo.toFloat(),
            steps = steps,
            value = sliderValue,
            enabled = enabled,
            onValueChangeFinished = { onValueChange(sliderValue.roundToInt()) },
            onValueChange = { sliderValue = it },
            interactionSource = interactionSource,
        )
    }
}


@Composable
fun Slider(
    label: String,
    initialValue: () -> Int,
    valueFrom: Int,
    valueTo: Int,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    @IntRange(from = 0) steps: Int = (valueTo - valueFrom - 1).coerceAtLeast(0),
    labelFormatter: (Int) -> String,
    onValueChange: (Int) -> Unit,
) {
    var value by rememberSaveable { mutableIntStateOf(initialValue()) }

    Slider(
        label = label,
        value = value,
        valueFrom = valueFrom,
        valueTo = valueTo,
        modifier = modifier,
        enabled = enabled,
        steps = steps,
        labelFormatter = labelFormatter,
        onValueChange = onValueChange,
    )
}
