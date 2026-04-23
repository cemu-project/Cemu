@file:OptIn(ExperimentalMaterial3Api::class)

package info.cemu.cemu.common.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.dropUnlessResumed
import info.cemu.cemu.R

@Composable
fun ScreenContent(
    appBarText: String,
    snackbarHost: @Composable () -> Unit = {},
    navigateBack: () -> Unit,
    actions: @Composable RowScope.() -> Unit = {},
    contentModifier: Modifier = Modifier.padding(8.dp),
    contentVerticalArrangement: Arrangement.Vertical = Arrangement.Top,
    contentHorizontalAlignment: Alignment.Horizontal = Alignment.Start,
    content: @Composable ColumnScope.() -> Unit,
) {
    ScreenContentGeneric(
        appBarTitle = { DefaultAppBarTitle(appBarText) },
        snackbarHost = snackbarHost,
        navigateBack = navigateBack,
        actions = actions,
    ) {
        Column(
            verticalArrangement = contentVerticalArrangement,
            horizontalAlignment = contentHorizontalAlignment,
            modifier = contentModifier.verticalScroll(rememberScrollState()),
        ) {
            content()
        }
    }
}

@Composable
fun DefaultAppBarTitle(appBarText: String) {
    Text(
        maxLines = 1,
        overflow = TextOverflow.Ellipsis,
        text = appBarText,
        fontSize = 18.sp,
    )
}

@Composable
fun ScreenContentLazy(
    appBarText: String,
    navigateBack: () -> Unit,
    snackbarHost: @Composable () -> Unit = {},
    actions: @Composable RowScope.() -> Unit = {},
    contentModifier: Modifier = Modifier.padding(8.dp),
    contentVerticalArrangement: Arrangement.Vertical = Arrangement.Top,
    content: LazyListScope.() -> Unit,
) {
    ScreenContentGeneric(
        snackbarHost = snackbarHost,
        appBarTitle = { DefaultAppBarTitle(appBarText) },
        navigateBack = navigateBack,
        actions = actions,
    ) {
        LazyColumn(
            verticalArrangement = contentVerticalArrangement,
            modifier = contentModifier,
        ) {
            content()
        }
    }
}

@Composable
fun ScreenContentLazy(
    appBarTitle: @Composable () -> Unit,
    navigateBack: () -> Unit,
    snackbarHost: @Composable () -> Unit = {},
    actions: @Composable RowScope.() -> Unit = {},
    contentModifier: Modifier = Modifier.padding(8.dp),
    contentVerticalArrangement: Arrangement.Vertical = Arrangement.Top,
    content: LazyListScope.() -> Unit,
) {
    ScreenContentGeneric(
        snackbarHost = snackbarHost,
        appBarTitle = appBarTitle,
        navigateBack = navigateBack,
        actions = actions,
    ) {
        LazyColumn(
            verticalArrangement = contentVerticalArrangement,
            modifier = contentModifier,
        ) {
            content()
        }
    }
}

@Composable
private fun ScreenContentGeneric(
    navigateBack: () -> Unit,
    appBarTitle: @Composable () -> Unit,
    snackbarHost: @Composable () -> Unit = {},
    actions: @Composable RowScope.() -> Unit = {},
    content: @Composable () -> Unit,
) {
    Scaffold(
        modifier = Modifier.fillMaxSize(),
        snackbarHost = snackbarHost,
        topBar = {
            TopAppBar(
                actions = actions,
                title = appBarTitle,
                navigationIcon = {
                    IconButton(onClick = dropUnlessResumed { navigateBack() }) {
                        Icon(
                            painter = painterResource(R.drawable.ic_arrow_back),
                            contentDescription = null,
                        )
                    }
                },
            )
        },
    ) { scaffoldPadding ->
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(scaffoldPadding)
        ) {
            content()
        }
    }
}