package info.cemu.cemu.about

import androidx.navigation.NavGraphBuilder
import androidx.navigation.NavHostController
import androidx.navigation.compose.composable
import kotlinx.serialization.Serializable

@Serializable
object AboutCemuRoute

fun NavGraphBuilder.aboutCemuNavigation(navController: NavHostController) {
    composable<AboutCemuRoute> {
        AboutCemuScreen { navController.popBackStack() }
    }
}
