package info.cemu.cemu.titlemanager

import androidx.navigation.NavGraphBuilder
import androidx.navigation.NavHostController
import androidx.navigation.compose.composable
import kotlinx.serialization.Serializable

@Serializable
object TitleManagerRoute

fun NavGraphBuilder.titleManagerNavigation(navController: NavHostController) {
    composable<TitleManagerRoute> {
        TitleManagerScreen({ navController.popBackStack() })
    }
}
