package info.cemu.cemu.common.ui.components

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
fun Toggle(
    label: String,
    checked: Boolean,
    onCheckedChanged: (Boolean) -> Unit,
    description: String? = null,
    enabled: Boolean = true,
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier
            .fillMaxWidth()
            .clickable(enabled = enabled) { onCheckedChanged(!checked) }
            .alpha(if (enabled) 1f else 0.6f),
    ) {
        Column(
            modifier = Modifier
                .weight(1.0f)
                .padding(8.dp),
            verticalArrangement = Arrangement.Center,
        ) {
            Text(
                text = label,
                fontWeight = FontWeight.Medium,
                fontSize = 20.sp,
            )
            if (description != null) {
                Text(
                    text = description,
                    fontSize = 14.sp,
                    modifier = Modifier.padding(top = 8.dp),
                )
            }
        }
        Switch(
            modifier = Modifier.padding(end = 8.dp, top = 8.dp, bottom = 8.dp),
            checked = checked,
            onCheckedChange = null,
            enabled = enabled,
        )
    }
}

@Composable
fun Toggle(
    label: String,
    initialCheckedState: () -> Boolean,
    onCheckedChanged: (Boolean) -> Unit,
    description: String? = null,
    enabled: Boolean = true,
) {
    var checked by rememberSaveable { mutableStateOf(initialCheckedState()) }

    Toggle(
        label = label,
        checked = checked,
        onCheckedChanged = {
            checked = it
            onCheckedChanged(it)
        },
        description = description,
        enabled = enabled,
    )
}