package info.cemu.cemu.common.ui.components

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.LocalTextStyle
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.input.PlatformImeOptions
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import info.cemu.cemu.R


@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun FilledSearchToolbar(
    query: String,
    hint: String,
    onValueChange: (String) -> Unit,
    actions: @Composable RowScope.() -> Unit = {},
) {
    var searchBarActive by remember { mutableStateOf(false) }

    BackHandler(enabled = searchBarActive) {
        onValueChange("")
        searchBarActive = false
    }

    TopAppBar(
        actions = actions,
        title = {
            Card(
                onClick = { searchBarActive = true },
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.surfaceContainerHighest,
                ),
                modifier = Modifier
                    .fillMaxWidth()
                    .height(IntrinsicSize.Min)
                    .padding(8.dp),
                shape = RoundedCornerShape(24.dp),
            ) {
                Box(
                    modifier = Modifier
                        .fillMaxHeight()
                        .padding(8.dp),
                    contentAlignment = Alignment.CenterStart,
                ) {
                    if (!searchBarActive) {
                        SearchToolbarHint(hint)
                    } else {
                        SearchToolbarInput(
                            trailingIcon = {
                                IconButton(
                                    onClick = {
                                        onValueChange("")
                                        searchBarActive = false
                                    },
                                    modifier = Modifier
                                        .padding(horizontal = 8.dp)
                                        .size(32.dp)
                                ) {
                                    Icon(
                                        painter = painterResource(R.drawable.ic_arrow_back),
                                        contentDescription = null
                                    )
                                }
                            },
                            value = query,
                            hint = hint,
                            onValueChange = onValueChange,
                        )
                    }
                }
            }
        },
    )
}

@Composable
fun SearchToolbarInput(
    value: String,
    hint: String,
    trailingIcon: @Composable () -> Unit = {},
    onValueChange: (String) -> Unit,
) {
    val focusRequester = remember { FocusRequester() }
    Row(
        verticalAlignment = Alignment.CenterVertically,
    ) {
        trailingIcon()
        BasicTextField(
            modifier = Modifier
                .fillMaxHeight()
                .focusRequester(focusRequester)
                .weight(1.0f),
            cursorBrush = SolidColor(LocalContentColor.current),
            value = value,
            keyboardOptions = KeyboardOptions(
                platformImeOptions = PlatformImeOptions("flagNoFullscreen|flagNoExtractUi")
            ),
            singleLine = true,
            textStyle = LocalTextStyle.current.copy(
                textAlign = TextAlign.Start,
                color = LocalContentColor.current,
            ),
            decorationBox = { innerTextField ->
                if (value.isEmpty()) {
                    SearchToolbarHint(
                        hint = hint,
                        color = LocalContentColor.current.copy(alpha = 0.6f)
                    )
                }
                innerTextField()
            },
            onValueChange = onValueChange,
        )
        if (value.isNotEmpty()) {
            IconButton(
                onClick = { onValueChange("") },
                modifier = Modifier
                    .padding(horizontal = 8.dp)
                    .size(32.dp)
            ) {
                Icon(
                    painter = painterResource(R.drawable.ic_close_small),
                    contentDescription = null
                )
            }
        }
        LaunchedEffect(Unit) {
            focusRequester.requestFocus()
        }
    }
}

@Composable
fun SearchToolbarHint(
    hint: String,
    color: Color = LocalContentColor.current,
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(
            modifier = Modifier
                .padding(horizontal = 8.dp)
                .size(24.dp),
            painter = painterResource(R.drawable.ic_search),
            tint = color,
            contentDescription = null
        )
        Text(
            text = hint,
            fontSize = 16.sp,
            color = color,
        )
    }
}