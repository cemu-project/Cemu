package info.cemu.cemu.games

import androidx.compose.foundation.Image
import androidx.compose.foundation.border
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import info.cemu.cemu.R
import info.cemu.cemu.nativeinterface.NativeGameTitles

private fun Modifier.iconBorder(borderColor: Color) =
    border(
        width = Dp.Hairline,
        color = borderColor,
        shape = RoundedCornerShape(8.dp)
    ).clip(RoundedCornerShape(8.dp))

@Composable
fun GameIcon(
    game: NativeGameTitles.Game,
    modifier: Modifier,
) {
    val borderColor = MaterialTheme.colorScheme.onSurface

    if (game.icon != null) {
        Image(
            modifier = modifier.iconBorder(borderColor),
            bitmap = game.icon!!,
            contentDescription = null
        )
    } else {
        Icon(
            modifier = modifier.iconBorder(borderColor),
            painter = painterResource(R.drawable.ic_question_mark),
            contentDescription = null
        )
    }
}