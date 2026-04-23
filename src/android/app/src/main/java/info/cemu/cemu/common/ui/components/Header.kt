package info.cemu.cemu.common.ui.components

import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
fun Header(text: String?, modifier: Modifier = Modifier) {
    Text(
        modifier = modifier.padding(horizontal = 8.dp, vertical = 16.dp),
        text = text ?: "",
        fontSize = 24.sp,
        fontWeight = FontWeight.Bold,
    )
}