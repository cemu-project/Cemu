@file:OptIn(ExperimentalMaterial3Api::class)

package info.cemu.cemu.graphicpacks

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.contentColorFor
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.dropUnlessResumed
import androidx.lifecycle.viewmodel.compose.viewModel
import info.cemu.cemu.R
import info.cemu.cemu.common.ui.components.DefaultAppBarTitle
import info.cemu.cemu.common.ui.components.ScreenContentLazy
import info.cemu.cemu.common.ui.components.SearchToolbarInput
import info.cemu.cemu.common.ui.components.SingleSelection
import info.cemu.cemu.common.ui.extensions.showMessage
import info.cemu.cemu.common.ui.localization.tr

@Composable
fun GraphicPacksScreen(
    navigateBack: () -> Unit,
    graphicPacksViewModel: GraphicPacksViewModel = viewModel(),
) {
    val graphicPackDataNodes by graphicPacksViewModel.graphicPackDataNodes.collectAsState()
    val query by graphicPacksViewModel.filterText.collectAsState()
    val installedOnly by graphicPacksViewModel.installedOnly.collectAsState()
    var showGraphicPackSearch by rememberSaveable { mutableStateOf(false) }
    val isDownloading by graphicPacksViewModel.isDownloading.collectAsState()
    val downloadStatus by graphicPacksViewModel.downloadStatus.collectAsState()
    val snackbarHostState = remember { SnackbarHostState() }
    val context = LocalContext.current
    val currentNodeState = graphicPacksViewModel.currentNode.collectAsState()
    val currentNode = currentNodeState.value
    val graphicPackDataState = graphicPacksViewModel.currentDataGraphicPack.collectAsState()
    val graphicPackData = graphicPackDataState.value

    LaunchedEffect(Unit) {
        graphicPacksViewModel.events.collect {
            val notificationMessage = downloadEventToNotificationString(it) ?: return@collect
            snackbarHostState.showMessage(this@LaunchedEffect, notificationMessage)
        }
    }

    fun handleBack() {
        if (showGraphicPackSearch) {
            showGraphicPackSearch = false
            return
        }

        if (!currentNodeState.value.isRoot()) {
            graphicPacksViewModel.navigateBack()
            return
        }

        navigateBack()
    }

    BackHandler(enabled = showGraphicPackSearch || !currentNodeState.value.isRoot()) {
        handleBack()
    }

    ScreenContentLazy(
        snackbarHost = { SnackbarHost(hostState = snackbarHostState) },
        actions = {
            if (currentNode.isRoot()) {
                GraphicPacksRootSectionActions(
                    showMainActions = !showGraphicPackSearch,
                    onSearchClicked = {
                        showGraphicPackSearch = true
                    },
                    onDownloadClicked = { graphicPacksViewModel.downloadNewUpdate(context) },
                    installedOnlyChecked = installedOnly,
                    installedOnlyValueChange = graphicPacksViewModel::setInstalledOnly,
                )
            }
        },
        appBarTitle = {
            Box(
                modifier = Modifier
                    .height(IntrinsicSize.Min)
                    .padding(8.dp),
            ) {
                if (showGraphicPackSearch && currentNode.isRoot()) {
                    SearchToolbarInput(
                        value = query,
                        onValueChange = graphicPacksViewModel::setFilterText,
                        hint = tr("Search graphic packs"),
                    )
                } else {
                    DefaultAppBarTitle(currentNode.name ?: tr("Graphic packs"))
                }
            }
        },
        navigateBack = ::handleBack,
    ) {
        if (showGraphicPackSearch && currentNode.isRoot()) {
            graphicPackDataSearchItems(
                nodes = graphicPackDataNodes,
                onClick = { graphicPacksViewModel.navigateTo(it) },
            )
            return@ScreenContentLazy
        }

        if (currentNode is GraphicPackSectionNode) {
            graphicPackSectionItems(
                installedOnly = installedOnly,
                nodes = currentNode.children,
                onClick = { graphicPacksViewModel.navigateTo(it) },
            )
        }

        if (graphicPackData != null) {
            graphicPackDataNodeItem(
                graphicPackData = graphicPackData,
                onSetCurrentGraphicPackActive = graphicPacksViewModel::setCurrentGraphicPackActive,
                onSetCurrentGraphicPackActivePreset = graphicPacksViewModel::setCurrentGraphicPackActivePreset
            )
        }

    }

    if (isDownloading) {
        GraphicPacksDownloadDialog(
            onCancelRequest = {
                graphicPacksViewModel.cancelDownload()
            },
            text = downloadStatusToDialogTextString(downloadStatus),
        )
    }
}

private fun downloadStatusToDialogTextString(downloadStatus: GraphicPacksDownloadStatus?): String =
    when (downloadStatus) {
        GraphicPacksDownloadStatus.CHECKING_VERSION -> tr("Checking version...")
        GraphicPacksDownloadStatus.DOWNLOADING -> tr("Downloading graphic packs...")
        GraphicPacksDownloadStatus.EXTRACTING -> tr("Extracting...")
        else -> tr("Processing...")
    }

private fun downloadEventToNotificationString(event: GraphicPacksEvent?): String? =
    when (event) {
        GraphicPacksEvent.DOWNLOAD_FINISHED -> tr("Downloaded latest graphic packs")
        GraphicPacksEvent.DOWNLOAD_ERROR -> tr("Failed to download graphic packs")
        GraphicPacksEvent.NO_UPDATES_AVAILABLE -> tr("No updates available.")
        else -> null
    }

@Composable
private fun GraphicPacksDownloadDialog(
    onCancelRequest: () -> Unit,
    text: String,
) {
    AlertDialog(
        title = {
            Text(text = tr("Graphic packs download"))
        },
        text = {
            Column(modifier = Modifier.padding(vertical = 8.dp)) {
                Text(
                    text = text,
                    modifier = Modifier.padding(bottom = 16.dp)
                )
                LinearProgressIndicator(
                    modifier = Modifier.fillMaxWidth(),
                )
            }
        },
        onDismissRequest = {},
        confirmButton = {
            TextButton(
                onClick = {
                    onCancelRequest()
                }
            ) {
                Text(tr("Cancel"))
            }
        }
    )
}

@Composable
private fun GraphicPacksRootSectionActions(
    showMainActions: Boolean,
    onSearchClicked: () -> Unit,
    onDownloadClicked: () -> Unit,
    installedOnlyChecked: Boolean,
    installedOnlyValueChange: (Boolean) -> Unit,
) {
    if (showMainActions) {
        IconButton(
            onClick = onSearchClicked
        ) {
            Icon(
                painter = painterResource(R.drawable.ic_search),
                contentDescription = null
            )
        }
        IconButton(
            onClick = onDownloadClicked
        ) {
            Icon(
                painter = painterResource(R.drawable.ic_download),
                contentDescription = null
            )
        }
    }
    var showMoreOptions by rememberSaveable { mutableStateOf(false) }
    IconButton(
        onClick = { showMoreOptions = true }
    ) {
        Icon(
            painter = painterResource(R.drawable.ic_more_vert),
            contentDescription = null
        )
    }
    DropdownMenu(
        expanded = showMoreOptions,
        onDismissRequest = { showMoreOptions = false }
    ) {
        DropdownMenuItem(
            onClick = {
                installedOnlyValueChange(!installedOnlyChecked)
            },
            text = {
                Text(tr("Installed only"))
            },
            trailingIcon = {
                Checkbox(
                    checked = installedOnlyChecked,
                    onCheckedChange = null
                )
            }
        )
    }
}

private fun LazyListScope.graphicPackDataSearchItems(
    nodes: List<GraphicPackDataNode>,
    onClick: (GraphicPackDataNode) -> Unit,
) {
    items(nodes) {
        Row(
            modifier = Modifier
                .animateItem()
                .clickable(onClick = dropUnlessResumed { onClick(it) })
                .padding(8.dp)
                .fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            GraphicPackDataListItemIcon(it.enabled)
            Column {
                Text(
                    modifier = Modifier.padding(horizontal = 8.dp),
                    text = it.name ?: "",
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                Text(
                    modifier = Modifier.padding(horizontal = 8.dp),
                    text = it.path,
                )
            }
        }
    }
}

private fun LazyListScope.graphicPackDataNodeItem(
    onSetCurrentGraphicPackActive: (Boolean) -> Unit,
    onSetCurrentGraphicPackActivePreset: (Int, String) -> Unit,
    graphicPackData: GraphicPackData
) {
    item {
        Row(
            modifier = Modifier.padding(8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(text = tr("Enabled"))
            Switch(
                modifier = Modifier.padding(horizontal = 8.dp),
                checked = graphicPackData.active,
                onCheckedChange = onSetCurrentGraphicPackActive,
            )
        }
    }
    item {
        Text(
            modifier = Modifier.padding(8.dp),
            text = graphicPackData.description
        )
    }
    items(items = graphicPackData.presets) {
        SingleSelection(
            modifier = Modifier.animateItem(),
            label = it.category ?: tr("Active preset"),
            choices = it.choices,
            choice = it.activeChoice,
            onChoiceChanged = { activePreset ->
                onSetCurrentGraphicPackActivePreset(it.index, activePreset)
            }
        )
    }
}

private fun LazyListScope.graphicPackSectionItems(
    nodes: List<GraphicPackNode>,
    installedOnly: Boolean,
    onClick: (GraphicPackNode) -> Unit,
) {
    items(
        items = if (installedOnly) nodes.filter { it.titleIdInstalled } else nodes,
    ) {
        GraphicPackListItem(
            label = it.name,
            onClick = dropUnlessResumed { onClick(it) },
            modifier = Modifier.animateItem(),
        ) {
            when (it) {
                is GraphicPackSectionNode -> GraphicPackSectionListItemIcon(it.enabledGraphicPacksCount)
                is GraphicPackDataNode -> GraphicPackDataListItemIcon(it.enabled)
            }
        }
    }
}

@Composable
private fun GraphicPackDataListItemIcon(isEnabled: Boolean) {
    GraphicPackListItemIcon(
        painter = painterResource(R.drawable.ic_package_2),
        showExtraInfo = isEnabled,
    ) {
        Icon(
            modifier = Modifier
                .background(MaterialTheme.colorScheme.primary, CircleShape)
                .size(16.dp),
            painter = painterResource(R.drawable.ic_check),
            tint = contentColorFor(MaterialTheme.colorScheme.primary),
            contentDescription = null
        )
    }
}

@Composable
private fun GraphicPackSectionListItemIcon(numberOfEnabledPacks: Int) {
    GraphicPackListItemIcon(
        painter = painterResource(R.drawable.ic_lists),
        showExtraInfo = numberOfEnabledPacks > 0,
    ) {
        Text(
            textAlign = TextAlign.Center,
            text = if (numberOfEnabledPacks < 99) numberOfEnabledPacks.toString() else "99+",
            modifier = Modifier
                .background(MaterialTheme.colorScheme.primary, RoundedCornerShape(2.dp))
                .padding(horizontal = 2.dp),
            color = contentColorFor(MaterialTheme.colorScheme.primary),
        )
    }
}

@Composable
private fun GraphicPackListItemIcon(
    painter: Painter,
    showExtraInfo: Boolean,
    extraInfoContent: @Composable () -> Unit,
) {
    Box {
        Icon(
            modifier = Modifier.size(28.dp),
            painter = painter,
            contentDescription = null
        )
        if (showExtraInfo) {
            Box(modifier = Modifier.align(Alignment.BottomEnd)) {
                extraInfoContent()
            }
        }
    }
}

@Composable
private fun GraphicPackListItem(
    label: String?,
    onClick: () -> Unit,
    modifier: Modifier,
    icon: @Composable () -> Unit,
) {
    Row(
        modifier = modifier
            .clickable(onClick = onClick)
            .padding(8.dp)
            .fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        icon()
        Text(
            modifier = Modifier
                .weight(1.0f)
                .padding(horizontal = 8.dp),
            text = label ?: "",
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
    }
}
