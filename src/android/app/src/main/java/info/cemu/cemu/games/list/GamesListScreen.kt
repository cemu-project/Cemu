@file:OptIn(
    ExperimentalMaterial3Api::class, ExperimentalFoundationApi::class,
)

package info.cemu.cemu.games.list

import android.content.Context
import android.content.Intent
import android.provider.DocumentsContract
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.pulltorefresh.PullToRefreshDefaults
import androidx.compose.material3.pulltorefresh.pullToRefresh
import androidx.compose.material3.pulltorefresh.rememberPullToRefreshState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import info.cemu.cemu.R
import info.cemu.cemu.common.ui.components.FilledSearchToolbar
import info.cemu.cemu.common.ui.extensions.showMessage
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.games.GameIcon
import info.cemu.cemu.nativeinterface.NativeActiveSettings
import info.cemu.cemu.nativeinterface.NativeGameTitles
import info.cemu.cemu.nativeinterface.NativeGameTitles.Game
import info.cemu.cemu.provider.DocumentsProvider
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import java.io.File
import androidx.compose.material3.DropdownMenuItem as MaterialDropdownMenuItem

@Composable
fun GamesListScreen(
    gamesListViewModel: GamesListViewModel = viewModel(),
    goToGameDetails: (Game) -> Unit,
    goToGameEditProfile: (Game) -> Unit,
    startGame: (Game) -> Unit,
    goToSettings: () -> Unit,
    goToTitleManager: () -> Unit,
    goToGraphicPacks: () -> Unit,
    goToAboutCemu: () -> Unit,
    tryCreateShortcut: (Game) -> Boolean,
) {
    val lifecycleOwner = LocalLifecycleOwner.current
    val lifecycleState by lifecycleOwner.lifecycle.currentStateFlow.collectAsState()
    val coroutineScope = rememberCoroutineScope()
    var refreshing by remember { mutableStateOf(false) }
    val snackbarHostState = remember { SnackbarHostState() }
    val gameSearchQuery by gamesListViewModel.filterText.collectAsStateWithLifecycle()
    val games by gamesListViewModel.games.collectAsStateWithLifecycle()
    val context = LocalContext.current

    val state = rememberPullToRefreshState()

    LaunchedEffect(lifecycleState) {
        if (lifecycleState == Lifecycle.State.RESUMED && gamesListViewModel.gamePathsHaveChanged())
            gamesListViewModel.refreshGames()
    }

    DisposableEffect(lifecycleOwner) {
        onDispose {
            gamesListViewModel.setFilterText("")
        }
    }

    Scaffold(
        snackbarHost = { SnackbarHost(hostState = snackbarHostState) },
        topBar = {
            FilledSearchToolbar(
                actions = {
                    GameListToolBarActionsMenu(
                        goToSettings = goToSettings,
                        goToTitleManager = goToTitleManager,
                        goToGraphicPacks = goToGraphicPacks,
                        goToAboutCemu = goToAboutCemu,
                        openCemuFolder = {
                            if (!tryOpenCemuFolder(context)) {
                                snackbarHostState.showMessage(
                                    coroutineScope,
                                    tr("Failed to open Cemu folder")
                                )
                            }
                        },
                        shareLogFile = {
                            if (!logFileExists()) {
                                snackbarHostState.showMessage(
                                    coroutineScope,
                                    tr("Log file doesn't exist")
                                )
                                return@GameListToolBarActionsMenu
                            }

                            if (!tryShareLogFile(context)) {
                                snackbarHostState.showMessage(
                                    coroutineScope,
                                    tr("Failed to open log file")
                                )
                            }
                        },
                    )
                },
                hint = tr("Search games"),
                query = gameSearchQuery,
                onValueChange = gamesListViewModel::setFilterText
            )
        },
    ) { scaffoldPadding ->
        Box(
            modifier = Modifier
                .padding(scaffoldPadding)
                .fillMaxSize()
                .pullToRefresh(
                    isRefreshing = refreshing,
                    state = state,
                    onRefresh = {
                        coroutineScope.launch {
                            refreshing = true
                            gamesListViewModel.refreshGames()
                            delay(1500)
                            refreshing = false
                        }
                    },
                ),
        ) {
            GameList(
                games = games,
                setFavorite = gamesListViewModel::setGameTitleFavorite,
                deleteShaderCaches = {
                    gamesListViewModel.removeShadersForGame(it)
                    snackbarHostState.showMessage(coroutineScope, tr("Shader caches removed"))
                },
                startGame = startGame,
                goToGameDetails = goToGameDetails,
                goToGameEditProfile = goToGameEditProfile,
                createShortcut = {
                    if (!tryCreateShortcut(it)) {
                        snackbarHostState.showMessage(
                            coroutineScope,
                            tr("Couldn't create shortcut for game")
                        )
                    }
                },
            )

            PullToRefreshDefaults.Indicator(
                modifier = Modifier.align(Alignment.TopCenter),
                isRefreshing = refreshing,
                state = state,
                containerColor = MaterialTheme.colorScheme.surfaceVariant,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

@Composable
private fun GameList(
    games: List<Game>,
    startGame: (Game) -> Unit,
    goToGameDetails: (Game) -> Unit,
    goToGameEditProfile: (Game) -> Unit,
    setFavorite: (Game, Boolean) -> Unit,
    createShortcut: (Game) -> Unit,
    deleteShaderCaches: (Game) -> Unit,
) {
    LazyVerticalGrid(
        modifier = Modifier
            .padding(8.dp)
            .fillMaxSize(),
        columns = GridCells.Adaptive(620.dp),
    ) {
        items(items = games, key = { it.path }) { game ->
            var showDeleteShaderConfirmationDialog by remember { mutableStateOf(false) }
            GameListItem(
                modifier = Modifier.animateItem(),
                game = game,
                onStartGame = startGame,
                onIsFavoriteChanged = { isFavorite ->
                    setFavorite(game, isFavorite)
                },
                onEditGameProfile = {
                    goToGameEditProfile(game)
                },
                onRemoveShaderCaches = { showDeleteShaderConfirmationDialog = true },
                onAboutTitle = {
                    goToGameDetails(game)
                },
                onCreateShortcut = {
                    createShortcut(game)
                },
            )

            if (showDeleteShaderConfirmationDialog) {
                ShaderCachesConfirmationDialog(
                    gameName = game.name ?: "",
                    onDismissRequest = { showDeleteShaderConfirmationDialog = false },
                    onConfirm = {
                        deleteShaderCaches(game)
                        showDeleteShaderConfirmationDialog = false
                    },
                )
            }
        }
    }
}

@Composable
private fun ShaderCachesConfirmationDialog(
    gameName: String,
    onDismissRequest: () -> Unit,
    onConfirm: () -> Unit,
) {
    AlertDialog(
        title = { Text(tr("Remove shader caches")) },
        text = { Text(tr("Remove the shader caches for {0}?", gameName)) },
        dismissButton = { TextButton(onClick = onDismissRequest) { Text(tr("No")) } },
        onDismissRequest = onDismissRequest,
        confirmButton = { TextButton(onClick = onConfirm) { Text(tr("Yes")) } },
    )
}

@Composable
private fun GameListItem(
    onStartGame: (Game) -> Unit,
    onIsFavoriteChanged: (Boolean) -> Unit,
    onEditGameProfile: () -> Unit,
    onRemoveShaderCaches: () -> Unit,
    onAboutTitle: () -> Unit,
    onCreateShortcut: () -> Unit,
    game: Game,
    modifier: Modifier = Modifier,
) {
    var contextMenuExpanded by rememberSaveable { mutableStateOf(false) }
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = modifier
            .combinedClickable(
                onClick = { onStartGame(game) },
                onLongClick = {
                    contextMenuExpanded = true
                },
            )
            .padding(8.dp)
            .fillMaxWidth(),
    ) {
        Box {
            GameIcon(
                game = game,
                modifier = Modifier.size(60.dp),
            )
            if (game.isFavorite) {
                Icon(
                    modifier = Modifier
                        .align(Alignment.TopEnd)
                        .padding(2.dp)
                        .size(24.dp)
                        .clip(CircleShape)
                        .background(MaterialTheme.colorScheme.surfaceVariant),
                    painter = painterResource(R.drawable.ic_favorite),
                    tint = MaterialTheme.colorScheme.primary,
                    contentDescription = null
                )
            }
        }
        Text(
            modifier = Modifier.padding(horizontal = 8.dp),
            text = game.name ?: "",
            fontSize = 24.sp,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
        GameContextMenu(
            expanded = contextMenuExpanded,
            onDismissRequest = { contextMenuExpanded = false },
            game = game,
            onIsFavoriteChanged = onIsFavoriteChanged,
            onEditGameProfile = onEditGameProfile,
            onRemoveShaderCaches = onRemoveShaderCaches,
            onAboutTitle = onAboutTitle,
            onCreateShortcut = onCreateShortcut,
        )
    }
}

@Composable
private fun GameContextMenu(
    expanded: Boolean,
    onDismissRequest: () -> Unit,
    onIsFavoriteChanged: (Boolean) -> Unit,
    onEditGameProfile: () -> Unit,
    onRemoveShaderCaches: () -> Unit,
    onAboutTitle: () -> Unit,
    onCreateShortcut: () -> Unit,
    game: Game,
) {
    @Composable
    fun GameContextMenuItem(
        onClick: () -> Unit,
        text: String,
        enabled: Boolean = true,
        trailingIcon: @Composable (() -> Unit)? = null,
    ) {
        DropdownMenuItem(
            enabled = enabled,
            onClick = {
                onDismissRequest()
                onClick()
            },
            text = {
                Text(text = text)
            },
            trailingIcon = trailingIcon,
        )
    }
    DropdownMenu(expanded = expanded, onDismissRequest = onDismissRequest) {
        val gameTitleHasCaches = rememberSaveable {
            NativeGameTitles.titleHasShaderCacheFiles(game.titleId)
        }
        GameContextMenuItem(
            onClick = { onIsFavoriteChanged(!game.isFavorite) },
            text = tr("Favorite"),
            trailingIcon = { Checkbox(checked = game.isFavorite, onCheckedChange = null) },
        )
        GameContextMenuItem(
            onClick = onEditGameProfile,
            text = tr("Edit game profile"),
        )
        GameContextMenuItem(
            enabled = gameTitleHasCaches,
            onClick = onRemoveShaderCaches,
            text = tr("Remove shader caches")
        )
        GameContextMenuItem(
            onClick = onAboutTitle,
            text = tr("About title"),
        )
        GameContextMenuItem(
            onClick = onCreateShortcut,
            text = tr("Create shortcut"),
        )
    }
}

@Composable
private fun GameListToolBarActionsMenu(
    openCemuFolder: () -> Unit,
    shareLogFile: () -> Unit,
    goToSettings: () -> Unit,
    goToTitleManager: () -> Unit,
    goToGraphicPacks: () -> Unit,
    goToAboutCemu: () -> Unit,
) {
    var expandMenu by remember { mutableStateOf(false) }

    @Composable
    fun DropdownMenuItem(onClick: () -> Unit, text: String) {
        MaterialDropdownMenuItem(
            onClick = {
                onClick()
                expandMenu = false
            },
            text = { Text(text) },
        )
    }
    IconButton(
        modifier = Modifier.padding(end = 8.dp),
        onClick = { expandMenu = true },
    ) {
        Icon(
            painter = painterResource(R.drawable.ic_more_vert),
            contentDescription = null
        )
    }

    DropdownMenu(
        expanded = expandMenu,
        onDismissRequest = { expandMenu = false }
    ) {
        DropdownMenuItem(
            onClick = goToSettings,
            text = tr("Settings")
        )
        DropdownMenuItem(
            onClick = goToGraphicPacks,
            text = tr("Graphic packs")
        )
        DropdownMenuItem(
            onClick = goToTitleManager,
            text = tr("Title manager")
        )
        DropdownMenuItem(
            onClick = openCemuFolder,
            text = tr("Open Cemu folder")
        )
        DropdownMenuItem(
            onClick = shareLogFile,
            text = tr("Share log file"),
        )
        DropdownMenuItem(
            onClick = goToAboutCemu,
            text = tr("About Cemu"),
        )
    }
}

private const val LOG_FILE_NAME = "log.txt"

private fun logFileExists(): Boolean {
    return File(NativeActiveSettings.getUserDataPath()).resolve(LOG_FILE_NAME).isFile
}

private fun tryShareLogFile(context: Context): Boolean {
    try {
        val fileUri = DocumentsContract.buildDocumentUri(
            DocumentsProvider.AUTHORITY,
            DocumentsProvider.ROOT_ID + "/$LOG_FILE_NAME"
        )

        val documentFile = DocumentFile.fromSingleUri(context, fileUri)!!

        val intent = Intent(Intent.ACTION_SEND)
            .setDataAndType(documentFile.uri, "text/plain")
            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            .putExtra(Intent.EXTRA_STREAM, documentFile.uri)

        context.startActivity(Intent.createChooser(intent, null))

        return true
    } catch (_: Exception) {
        return false
    }
}

private fun tryOpenCemuFolder(context: Context): Boolean {
    try {
        val intent = Intent(Intent.ACTION_VIEW)
            .addCategory(Intent.CATEGORY_DEFAULT)
            .addFlags(
                Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
                        or Intent.FLAG_GRANT_READ_URI_PERMISSION
                        or Intent.FLAG_GRANT_PREFIX_URI_PERMISSION
                        or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
            )
        intent.data = DocumentsContract.buildRootUri(
            DocumentsProvider.AUTHORITY,
            DocumentsProvider.ROOT_ID
        )
        context.startActivity(intent)

        return true
    } catch (_: Exception) {
        return false
    }
}
