package info.cemu.cemu.settings.gamespath

import android.content.Intent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.basicMarquee
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Card
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.viewmodel.compose.viewModel
import info.cemu.cemu.R
import info.cemu.cemu.common.ui.components.ScreenContentLazy
import info.cemu.cemu.common.ui.localization.tr
import kotlinx.coroutines.launch

@Composable
fun GamePathsScreen(
    navigateBack: () -> Unit,
    gamesPathsViewModel: GamesPathsViewModel = viewModel(),
) {
    val context = LocalContext.current
    val coroutineScope = rememberCoroutineScope()
    val snackbarHostState = remember { SnackbarHostState() }
    val gamesPaths by gamesPathsViewModel.gamesPaths.collectAsState()
    val launcher =
        rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri ->
            if (uri == null) return@rememberLauncherForActivityResult
            context.contentResolver.takePersistableUriPermission(
                uri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
            )
            val documentFile =
                DocumentFile.fromTreeUri(context, uri) ?: return@rememberLauncherForActivityResult
            val gamesPath = documentFile.uri.toString()
            if (gamesPaths.contains(gamesPath)) {
                coroutineScope.launch {
                    snackbarHostState.currentSnackbarData?.dismiss()
                    snackbarHostState.showSnackbar(tr("Games path already added"))
                }
                return@rememberLauncherForActivityResult
            }
            gamesPathsViewModel.addGamesPath(gamesPath)
        }
    ScreenContentLazy(
        snackbarHost = { SnackbarHost(hostState = snackbarHostState) },
        appBarText = tr("Game paths"),
        navigateBack = navigateBack,
        actions = {
            IconButton(onClick = { launcher.launch(null) }) {
                Icon(
                    painter = painterResource(R.drawable.ic_add),
                    contentDescription = null
                )
            }
        },
    ) {
        items(items = gamesPaths, key = { it }) {
            GamePathsListItem(
                modifier = Modifier.animateItem(),
                gamesPath = it,
                onDelete = { gamesPathsViewModel.removeGamesPath(it) }
            )
        }
    }
}

@Composable
fun GamePathsListItem(
    modifier: Modifier,
    gamesPath: String,
    onDelete: () -> Unit,
) {
    Card(modifier = modifier.padding(8.dp)) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .fillMaxWidth(),
        ) {
            Text(
                modifier = Modifier
                    .weight(1.0f)
                    .padding(horizontal = 12.dp, vertical = 8.dp)
                    .basicMarquee(),
                text = gamesPath,
                maxLines = 1,
            )
            IconButton(
                modifier = Modifier.padding(top = 8.dp, bottom = 8.dp, end = 8.dp),
                onClick = onDelete
            ) {
                Icon(
                    painter = painterResource(R.drawable.ic_delete),
                    contentDescription = null
                )
            }
        }
    }
}
