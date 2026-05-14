package info.cemu.cemu.graphicpacks

import androidx.navigation.NavGraphBuilder
import androidx.navigation.NavHostController
import androidx.navigation.compose.composable
import kotlinx.serialization.Serializable

@Serializable
object GraphicPacksRoute

fun NavGraphBuilder.graphicPacksNavigation(navController: NavHostController) {
    composable<GraphicPacksRoute> {
        GraphicPacksScreen(
            navigateBack = { navController.popBackStack() }
        )
    }
}
