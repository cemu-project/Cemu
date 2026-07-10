package info.cemu.cemu.games.details

import android.content.ClipData
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ClipEntry
import androidx.compose.ui.platform.LocalClipboard
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import info.cemu.cemu.common.ui.components.ScreenContent
import info.cemu.cemu.common.ui.localization.regionToString
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.games.GameIcon
import info.cemu.cemu.nativeinterface.NativeGameTitles.Game
import kotlinx.coroutines.launch
import java.time.LocalDate
import java.time.format.DateTimeFormatter
import java.time.format.FormatStyle

@Composable
fun GameDetailsScreen(game: Game?, navigateBack: () -> Unit) {
    if (game == null)
        return

    val scope = rememberCoroutineScope()
    val snackbarHostState = remember { SnackbarHostState() }
    val clipboard = LocalClipboard.current

    @Composable
    fun <T> TitleDetailsEntry(entryName: String, entryData: T?) {
        Column(
            modifier = Modifier
                .clickable {
                    scope.launch {
                        val data = entryData?.toString() ?: return@launch
                        val clipData = ClipData.newPlainText(entryName, data)
                        clipboard.setClipEntry(ClipEntry(clipData))
                        snackbarHostState.currentSnackbarData?.dismiss()
                        snackbarHostState.showSnackbar(tr("Copied to clipboard"))
                    }
                }
                .padding(horizontal = 8.dp, vertical = 16.dp)
                .fillMaxWidth()
        ) {
            Text(
                text = entryName,
                fontWeight = FontWeight.Bold,
                fontSize = 24.sp,
            )
            Text(
                text = entryData?.toString() ?: "",
                fontSize = 16.sp,
            )
        }
    }

    ScreenContent(
        snackbarHost = { SnackbarHost(hostState = snackbarHostState) },
        appBarText = tr("About title"),
        navigateBack = navigateBack,
    ) {
        GameIcon(
            game = game,
            modifier = Modifier.size(128.dp),
        )
        TitleDetailsEntry(entryName = tr("Title name"), entryData = game.name)
        TitleDetailsEntry(entryName = tr("Title ID"), entryData = game.titleId)
        TitleDetailsEntry(entryName = tr("Version"), entryData = game.version)
        TitleDetailsEntry(entryName = tr("DLC"), entryData = game.dlc)
        TitleDetailsEntry(
            entryName = tr("You've played"),
            entryData = getTimePlayed(game)
        )
        TitleDetailsEntry(
            entryName = tr("Last played"),
            entryData = getLastPlayedDate(game)
        )
        TitleDetailsEntry(
            entryName = tr("Region"),
            entryData = regionToString(game.region)
        )
        TitleDetailsEntry(
            entryName = tr("Path"),
            entryData = game.path
        )
    }
}

private fun getTimePlayed(game: Game): String {
    if (game.minutesPlayed == 0) {
        return tr("Never played")
    }
    if (game.minutesPlayed < 60) {
        return tr("Minutes: {0}", game.minutesPlayed)
    }
    return tr(
        "Hours: {0} Minutes: {1}",
        game.minutesPlayed / 60,
        game.minutesPlayed % 60
    )
}

private val DateFormatter = DateTimeFormatter.ofLocalizedDate(
    FormatStyle.SHORT
)

private fun getLastPlayedDate(game: Game): String {
    if (game.lastPlayedYear.toInt() == 0) {
        return tr("Never played")
    }
    val lastPlayedDate = LocalDate.of(
        game.lastPlayedYear.toInt(),
        game.lastPlayedMonth.toInt(),
        game.lastPlayedDay.toInt()
    )
    return DateFormatter.format(lastPlayedDate)
}
