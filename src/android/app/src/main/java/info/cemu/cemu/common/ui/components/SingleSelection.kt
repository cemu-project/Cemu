package info.cemu.cemu.common.ui.components

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.sizeIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import info.cemu.cemu.common.ui.localization.tr

@Composable
fun SingleSelection(
    label: String,
    choice: String,
    choices: Collection<String>,
    modifier: Modifier = Modifier,
    isChoiceEnabled: (String) -> Boolean = { true },
    enabled: Boolean = true,
    onChoiceChanged: (String) -> Unit,
) {
    SingleSelection(
        label = label,
        choice = choice,
        choices = choices,
        modifier = modifier,
        choiceToString = { it },
        enabled = enabled,
        isChoiceEnabled = isChoiceEnabled,
        onChoiceChanged = onChoiceChanged,
    )
}


@Composable
fun <T> SingleSelection(
    label: String,
    initialChoice: () -> T,
    choices: Collection<T>,
    modifier: Modifier = Modifier,
    choiceToString: (T) -> String,
    isChoiceEnabled: (T) -> Boolean = { true },
    enabled: Boolean = true,
    onChoiceChanged: (T) -> Unit,
) {
    var choice by rememberSaveable { mutableStateOf(initialChoice()) }
    SingleSelection(
        label = label,
        choice = choice,
        choices = choices,
        modifier = modifier,
        isChoiceEnabled = isChoiceEnabled,
        enabled = enabled,
        choiceToString = choiceToString,
        onChoiceChanged = { newChoice ->
            choice = newChoice
            onChoiceChanged(newChoice)
        },
    )
}

@Composable
fun <T> SingleSelection(
    label: String,
    choice: T,
    choices: Collection<T>,
    choiceToString: (T) -> String,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    isChoiceEnabled: (T) -> Boolean = { true },
    onChoiceChanged: (T) -> Unit,
) {
    var showSelectDialog by rememberSaveable { mutableStateOf(false) }

    val clickableModifier = if (enabled) {
        Modifier.clickable { showSelectDialog = true }
    } else {
        Modifier
    }
    CompositionLocalProvider(
        LocalContentColor provides
                MaterialTheme.colorScheme.onSurface.copy(alpha = if (enabled) 1f else 0.38f)
    )
    {
        Column(
            modifier = modifier
                .fillMaxWidth()
                .then(clickableModifier)
                .padding(8.dp),
            verticalArrangement = Arrangement.Center,
        ) {
            Text(
                text = label,
                modifier = Modifier.padding(vertical = 8.dp),
                fontWeight = FontWeight.Medium,
                fontSize = 20.sp,
            )
            Text(
                text = choiceToString(choice),
                modifier = Modifier.padding(vertical = 8.dp),
                fontSize = 16.sp,
            )
        }
    }

    if (showSelectDialog) {
        SelectDialog(
            label = label,
            currentChoice = choice,
            choices = choices,
            choiceToString = choiceToString,
            onDismissRequest = { showSelectDialog = false },
            isChoiceEnabled = isChoiceEnabled,
            onChoiceChanged = onChoiceChanged
        )
    }
}

@Composable
private fun <T> SelectDialog(
    label: String,
    currentChoice: T,
    choices: Collection<T>,
    isChoiceEnabled: (T) -> Boolean,
    choiceToString: (T) -> String,
    onDismissRequest: () -> Unit,
    onChoiceChanged: (T) -> Unit,
) {
    Dialog(onDismissRequest = onDismissRequest) {
        Card(
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.surfaceContainer,
            ),
            modifier = Modifier
                .sizeIn(maxWidth = 560.dp, maxHeight = 560.dp)
                .fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
        ) {
            Text(
                modifier = Modifier.padding(
                    start = 16.dp,
                    end = 16.dp,
                    top = 16.dp,
                    bottom = 8.dp
                ),
                text = label,
                fontSize = 24.sp,
            )
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 4.dp, horizontal = 16.dp)
                    .weight(weight = 1.0f, fill = false)
                    .verticalScroll(rememberScrollState()),
            ) {
                choices.forEach { choice ->
                    Choice(
                        label = choiceToString(choice),
                        selected = currentChoice == choice,
                        isEnabled = isChoiceEnabled(choice),
                        onClick = {
                            onChoiceChanged(choice)
                            onDismissRequest()
                        },
                    )
                }
            }

            HorizontalDivider()

            TextButton(
                onClick = onDismissRequest,
                modifier = Modifier
                    .padding(8.dp)
                    .align(Alignment.End),
            ) {
                Text(tr("Cancel"))
            }
        }
    }
}

@Composable
private fun Choice(label: String, selected: Boolean, isEnabled: Boolean, onClick: () -> Unit) {
    Row(
        horizontalArrangement = Arrangement.spacedBy(4.dp),
        modifier = Modifier
            .clickable(isEnabled, onClick = onClick)
            .padding(vertical = 16.dp, horizontal = 8.dp)
            .fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        RadioButton(
            enabled = isEnabled,
            selected = selected,
            onClick = null,
        )
        Text(
            text = label,
            color = if (isEnabled) LocalContentColor.current
            else LocalContentColor.current.copy(alpha = 0.6f)
        )
    }
}
